#include "net/SocketAddress.hxx"
#include "net/SocketDescriptor.hxx"
#include "util/ByteOrder.hxx"
#include "util/PrintException.hxx"

#include <chrono>
#include <cstring>
#include <ctime>
#include <exception>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <sstream>
#include <thread>
#include <variant>
#include <vector>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#endif

#include <stdlib.h>

using std::byte;
using std::make_tuple;
using std::span;

class CliError : public std::runtime_error
{
public:
	CliError(const char *pmsg) : std::runtime_error(pmsg)
	{ }
	CliError(const std::string &msg) : std::runtime_error(msg)
	{ }
};

/// Parse the command line, return our parameters
static std::tuple<std::string, uint16_t, uint16_t, int16_t>
ParseCl(int argc, char **argv)
{
	if (5 != argc) {
		throw CliError("Four arguments expected");
	}

	uint16_t port = atoi(argv[2]);
	if (0 == port) {
		throw CliError("Couldn't parse port");
	}

	uint16_t fps = atoi(argv[3]);
	if (0 == fps) {
		throw CliError("Couldn't parse fps");
	}

	int16_t tau = atoi(argv[4]);
	// Arghhh... no way to distinguish between "0" and error

	return std::make_tuple(argv[1], port, fps, tau);
}

/// Connect to the MPD visualization server
static std::variant<std::monostate, SocketDescriptor>
Connect(const std::string &host, uint16_t port)
{
	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = ToBE16(port);

	if (0 >= inet_pton(AF_INET, host.c_str(), &addr.sin_addr)) {
		std::string msg = "Failed to parse '" + host + "' as a hostname (" +
			strerror(errno) + ")";
		throw CliError(msg);
	}

	SocketAddress sock_addr((const struct sockaddr*)&addr, sizeof(addr));

	SocketDescriptor sock;
	if (!sock.Create(AF_INET, SOCK_STREAM, 0)) {
		throw std::runtime_error("Faled to 'Create' the SocketDescriptor.");
	}

	if (sock.Connect(sock_addr)) {
		return sock;
	}

	return std::monostate { };
}

static std::tuple<uint8_t, uint8_t>
Handshake(SocketDescriptor &sock, uint16_t fps, int16_t tau)
{
	static byte buf[11] = {
		byte{0x00}, byte{0x00}, // message type
		byte{0x00}, byte{0x06}, // payload length
		byte{0x00}, byte{0x01}, // request version 0.1
	};

	uint16_t fpsn = ToBE16(fps);
	int16_t taun = ToBE16(tau);
	std::copy((byte*)&fpsn, (byte*)(&fpsn + 2), buf + 6);
	std::copy((byte*)&taun, (byte*)(&taun + 2), buf + 8);
	buf[10] = byte{0};

	ssize_t cb = sock.Write(span(buf, buf+ sizeof(buf)));
	if (0 >= cb) {
		throw std::runtime_error(strerror(errno));
	}
	if (cb != sizeof(buf)) {
		throw std::runtime_error("Incomplete write.");
	}

	cb = sock.Read(span(buf, buf + sizeof(buf)));
	if (0 >= cb) {
		throw std::runtime_error(strerror(errno));
	}

	byte *p = buf;
	uint16_t msgtype = FromBE16(*(uint16_t *)p); p += 2;
	if (0x0001 != msgtype) {
		throw std::runtime_error("Unexpected message type");
	}

	uint16_t msglen = FromBE16(*(uint16_t *)p); p += 2;
	if (0x0002 != msglen) {
		throw std::runtime_error("Unexpected message length");
	}

	uint8_t proto_ver_major = (uint8_t)*p++;
	uint8_t proto_ver_minor = (uint8_t)*p++;

	return make_tuple(proto_ver_major, proto_ver_minor);
}

/// Listen for FRAME messages, print-out bass/mids/trebs
static void
Listen(SocketDescriptor &sock)
{
	using namespace std;
	using namespace std::chrono;

	byte buf[8192];

	// this will hold num_chan * 8 floats for to compute a weighted average of
	// recent bass values-- will initialize on first FRAME
	vector<float> bass;
	// index of the "next" slot for a bass value
	size_t bass_idx = 0;

	const float WEIGHTS[] = { 1.67772f, 2.09715f, 2.62144f, 3.2768f, 4.096f, 5.12f, 6.4f, 8.0f };

	for (size_t i = 0; ; ++i) {
		ssize_t cb = sock.Read(span(buf, buf + sizeof(buf)));
		if (0 >= cb) {
			if (0 == errno) {
				cout << "MPD went away." << endl;
				return;
			}
			throw std::runtime_error(strerror(errno));
		}

		std::time_t now;
		if ((std::time_t)-1 == std::time(&now)) {
			throw std::runtime_error(strerror(errno));
		}

		if (cb == sizeof(buf)) {
			throw std::runtime_error("Buffer overflow!") ;
		}

		// Hmmm... let's begin parsing (tho I think for now I'll just be
		// interested in bass/mids/trebs as a crude manual test).
		byte *p = buf;

		uint32_t sentinel = FromBE32(*(uint32_t *)p);
		p += 4;
		if (0x63ac8403 != sentinel) {
			throw std::runtime_error("Missing sentinel!");
		}

		uint16_t msg_type = FromBE16(*(uint16_t*)p); p += 2;
		if (0x1000 != msg_type) {
			stringstream stm;
			stm << "Unexpected message type 0x" << hex << msg_type << "!";
			throw std::runtime_error(stm.str());
		}

		uint16_t msg_len	 = FromBE16(*(uint16_t*)p); p += 2;
		uint16_t num_samp	 = FromBE16(*(uint16_t*)p); p += 2;
		uint8_t	 num_chan	 = *(uint8_t*)p;		 p += 1;
		/*uint16_t sample_rate = FromBE16(*(uint16_t*)p);*/ p += 2;

		if (0 == bass.size()) {
			bass.resize(num_chan * 8, 0.0f);
		}

		// Skip over waveforms for now!
		p += num_samp * num_chan * 4;

		uint16_t num_freq = FromBE16(*(uint16_t*)p); p += 2;
		/*uint32_t tmp = FromBE32(*(uint32_t *)p);*/ p += 4;
		/*float freq_lo = *(float*)&tmp;*/
		/*tmp = FromBE32(*(uint32_t *)p);*/ p += 4;
		/*float freq_hi = *(float*)&tmp;*/

		/*uint16_t freq_off = FromBE16(*(uint16_t*)p);*/ p += 2;

		// Let's skip the Fourier coefficients....
		p += num_chan * num_freq * 8;
		// as well as the power spectra
		p += num_chan * num_freq * 4;

		auto now_ms = duration_cast<microseconds>(system_clock::now().time_since_epoch());
		cout << put_time(gmtime(&now), "%c %Z") << ": [" <<
			now_ms.count() << "](" <<
			msg_len << "bytes) ";

		// OK-- let's just grab bass/mids/trebs for each channel.
		float mean_bass = 0.0f, mean_mids = 0.0f, mean_trebs = 0.0f;
		for (uint8_t j = 0; j < num_chan; ++j) {

			if (j) {
				cout << " ";
			}

			uint32_t tmp = FromBE32(*(uint32_t *)p); p += 4;
			float this_bass = *(float*)&tmp;
			tmp = FromBE32(*(uint32_t *)p); p += 4;
			float this_mids = *(float*)&tmp;
			tmp = FromBE32(*(uint32_t *)p); p += 4;
			float this_trebs = *(float*)&tmp;

			mean_bass += this_bass;
			mean_mids += this_mids;
			mean_trebs += this_trebs;

			// record the in this channel for use below in beat detection
			bass[j * 8 + bass_idx] = this_bass;

			cout << this_bass << "/" << this_mids << "/" << this_trebs;
		}

		cout << " ";

		mean_bass  /= (float) num_chan;
		mean_mids  /= (float) num_chan;
		mean_trebs /= (float) num_chan;

		// beat detection-- very crude. We'll compute a weighted average of the
		// bass in each channel. Note that this caclulation will be incorrect
		// for the first seven frames-- meh 🤷
		float weighted_mean_bass = 0.0f;
		for (uint8_t j = 0; j < num_chan; ++j) {

			if (j) {
				cout << "/";
			}

			// Given the way we're indexing, the weighted sum will come in two
			// parts:

			// the first will be bass[bass_idx]*WEIGHTS[7] + ... + bass[0]*WEIGHTS[7-bass_idx]

			// the second will be bass[bass_idx+1]*WEIGHTS[0] + ... + bass[7]*WEIGHTS[6-idx]
			// when idx < 7

			float weighted_mean = 0.0f;
			for (ptrdiff_t k = bass_idx, n = 0; k >= 0; --k, ++n) {
				weighted_mean += bass[j*8+k] * WEIGHTS[7-n];
			}
			if (bass_idx < 7) {
				for (size_t k = bass_idx+1, n = 0; k < 8; ++k, ++n) {
					weighted_mean += bass[j*8+k] * WEIGHTS[n];
				}
			}

			weighted_mean /= 33.2891f; // Sum of weights

			cout << weighted_mean;

			weighted_mean_bass += weighted_mean;
		}

		bass_idx = (bass_idx + 1) % 8;

		cout << " ";

		// `weighted_mean_bass` is the average weighted average of the bass across
		// all channels-- this is what we use for our signal.
		weighted_mean_bass /= (float)num_chan;

		float thresh = weighted_mean_bass * 0.325f;
		if ((mean_bass - weighted_mean_bass) > thresh) {
			cout << " BEAT DETECTED";
		}
		cout << endl;
	}
}

/// Testing client for the visualization output plugin
/// Invoke as `run_vis mpd-host port fps time-offset`
int main(int argc, char **argv) {
	using namespace std;

	try {
		string mpd_host;
		int16_t tau;
		uint16_t port, fps;
		tie(mpd_host, port, fps, tau) = ParseCl(argc, argv);

		while (true) {

			auto conn = Connect(mpd_host, port);
			if (0 == conn.index()) {
				cout << "Failed to connect; sleeping for fifteen seconds & retrying (hit Ctrl-C to exit)." << endl;
				std::this_thread::sleep_for(15000ms);
				continue;
			}

			auto sock = std::get<SocketDescriptor>(conn);
			cout << "Connected." << endl;

			uint8_t major, minor;
			tie(major, minor) = Handshake(sock, fps, tau);
			cout << "Received protocol version " << (int)major <<
				"." << (int)minor << "." << endl;

			Listen(sock);
			cout << "Sleeping for thirty seconds & retrying (hit Ctrl-C to exit)." << endl;
			std::this_thread::sleep_for(30000ms);
		}
	} catch (const CliError &ex) {
		PrintException(ex);
		return 2;
	} catch (...) {
		PrintException(std::current_exception());
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
