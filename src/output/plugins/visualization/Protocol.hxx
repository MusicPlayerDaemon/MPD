// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef VISUALIZATION_PROTOCOL_HXX_INCLUDED
#define VISUALIZATION_PROTOCOL_HXX_INCLUDED

#include "LowLevelProtocol.hxx"
#include "SoundAnalysis.hxx"

#include <cstddef>
#include <cstdint>

namespace Visualization {

/**
 * \brief A parsed CLIHLO message
 *
 * \sa ParseCliHlo
 *
 *
 * The visualization \ref vis_out_protocol "protocol" begins with the client
 * connecting to the server & providing certain paramters of the sound analysis
 * it would like to receive. That is done through the CLIHLO message (which see
 * \a ref vis_out_protocol_proto_clihlo "here").
 *
 * See \a vis_out_protocol_timing "timing" for details on parameter tau.
 *
 *
 */

struct ClientHello {
	/// Major protocol version the client would like to speak
	uint8_t major_version;
	/// Minor protocol version the client would like to speak
	uint8_t minor_version;
	/// The number of sound analyses per second the client would like to
	/// receive (presumably the rate at which it is rendering frames, hence
	/// the name "fps")
	uint16_t requested_fps;
	/// The desired offset (named "tau" in the documentation) between song
	/// time and analysis time at each analysis performed
	int16_t tau;
};

enum class ParseResult {
	OK,
	NEED_MORE_DATA,
	ERROR,
};

/**
 * \brief Attempt to parse a \ref vis_out_protocol_proto_clihlo "CLIHLO" message
 * from the given buffer
 *
 * \param buf [in] An array of octets potentially containing the message
 *
 * \param length [in] The length of \a buf, in octets
 *
 * \param clihlo [out] A reference to a `client_hello_t` structure to be
 * filled-in on successful execution
 *
 * \return ParseResult::OK if the message was successfully parsed,
 * NEED_MORE_DATA if the message is incomplete, or ERROR if the message cannot
 * be ready from \a buf
 *
 *
 * CLIHLO is the first message in the protocol, sent by the client. See
 * \ref vis_out_protocol_proto_clihlo "the protocol specification" for details,
 * and \ref vis_out_protocol "Visualization Network Protocol" for discussion
 * of the protocol altogether.
 *
 *
 */

ParseResult
ParseClihlo(void *buf, size_t length, ClientHello &clihlo) noexcept;

/// Serialize an SRVHLO message to wire format
template <typename OutIter>
void
SerializeSrvhlo(std::byte major_ver, std::byte minor_ver, OutIter pout) {
	using std::byte;

	*pout++ = byte{0}; //
	*pout++ = byte{1}; // message type
	*pout++ = byte{0}; //
	*pout++ = byte{2}; // payload length
	*pout++ = major_ver;
	*pout++ = minor_ver;
	*pout++ = byte{0}; // check byte
}

/// Serialize a FRAME message header to wire format
template <typename OutIter>
OutIter
SerializeSoundInfoFrameHeader(uint8_t num_chan,
			      size_t num_samp,
			      size_t num_freq,
 OutIter pout) {

	using std::byte;

	// Start with the "magic number" allowing clients to "lock on" to the
	// stream of sound info frames in the event of an error.
	// See \ref vis_out_protocol_proto_msgs for details.
	*pout++ = byte{0x63};
	*pout++ = byte{0xac};
	*pout++ = byte{0x84};
	*pout++ = byte{0x03};

	*pout++ = byte{16};
	*pout++ = byte{0};

	return SerializeU16(17 + 4 * num_chan * (num_samp + 3 * num_freq + 3),
			    pout);
}

/// Serialize a FRAME message payload to wire format
template <typename OutIter>
void
SerializeSoundInfoFrameFooter(OutIter pout) {
	*pout = std::byte{0x00};
}

/// Serialize a FRAME message to wire format
template <typename OutIter>
void
SerializeSoundInfoFrame(const Visualization::SoundAnalysis &a,
			OutIter pout) {
	pout = SerializeSoundInfoFrameHeader(a.NumChan(), a.NumSamp(),
					     a.NumFreq(), pout);
	pout = a.SerializeSoundInfoFramePayload(pout);
	SerializeSoundInfoFrameFooter(pout);
}

} // namespace Visualization.

#endif // VISUALIZATION_PROTOCOL_HXX_INCLUDED
