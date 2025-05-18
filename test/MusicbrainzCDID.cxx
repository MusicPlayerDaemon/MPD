#include "fs/AllocatedPath.hxx"
#include "lib/cdio/Paranoia.hxx"
#include "util/StringCompare.hxx"
#include "util/StringSplit.hxx"
#include "util/NumberParser.hxx"
#include "util/ScopeExit.hxx"
#include "util/PrintException.hxx"
#include <cdio/cd_types.h>
#include <cstdint>
#include <vector>

extern "C" {
#include <libavutil/sha.h>
#include <libavutil/base64.h>
#include <libavutil/mem.h>
}

#include <stdio.h>

using std::string_view_literals::operator""sv;

struct TrackOffsets
{
	int firstTrackNumber = 1;
	int lastTrackNumber = -1;
	int leadIn = -1;
	std::vector<int> frameOffsets;

	bool isValid () const
	{
		return lastTrackNumber >= firstTrackNumber
			&& leadIn > 0
			&& frameOffsets.size() > 0;
	}
};

static const std::string
makeMusicBrainzIdWithOffsets (const TrackOffsets& trackOffsets)
{
	const size_t numBitsSha1 = 160;
	const size_t digestSize = numBitsSha1 / 8;
	uint8_t digest[digestSize];
	const size_t tempSize = 32;
	char temp[tempSize];
	struct AVSHA* sha1 = av_sha_alloc();

	AtScopeExit(sha1) { av_free(sha1); };

	av_sha_init(sha1, numBitsSha1);

	snprintf(temp, tempSize, "%02X", trackOffsets.firstTrackNumber);
	av_sha_update(sha1, (const uint8_t*)temp, strlen(temp));

	snprintf(temp, tempSize, "%02X", trackOffsets.lastTrackNumber);
	av_sha_update(sha1, (const uint8_t*)temp, strlen(temp));

	const size_t numTracksNeeded = 100; 

	if (trackOffsets.frameOffsets.size() > numTracksNeeded)
	{
		throw std::runtime_error("Too many tracks found");
	}

	for (size_t i = 0; i < trackOffsets.frameOffsets.size(); ++i)
	{
		int offset = trackOffsets.frameOffsets[i] + trackOffsets.leadIn;

		snprintf(temp, tempSize, "%08X", offset);
		av_sha_update(sha1, (const uint8_t*)temp, strlen(temp));
	}

	for (size_t i = trackOffsets.frameOffsets.size(); i < numTracksNeeded; ++i)
	{
		int offset = 0;

		snprintf(temp, tempSize, "%08X", offset);
		av_sha_update(sha1, (const uint8_t*)temp, strlen(temp));
	}

	av_sha_final(sha1, digest);

	const size_t outputSize = 128;
	char output[outputSize] = { 0 };

	av_base64_encode(output, outputSize, digest, digestSize);

	std::string outputString(output);

	std::replace(outputString.begin(), outputString.end(), '=', '-');
	std::replace(outputString.begin(), outputString.end(), '/', '_');
	std::replace(outputString.begin(), outputString.end(), '+', '.');

	return outputString;
}

static const TrackOffsets
getTrackOffsetsFromDevice_cdio (const AllocatedPath& device)
{
	const auto cdio = cdio_open(device.c_str(), DRIVER_UNKNOWN);
	if (cdio == nullptr)
		throw std::runtime_error("Failed to open CD drive");

	const auto drv = cdio_cddap_identify_cdio(cdio, 1, nullptr);
	if (drv == nullptr)
	{
		cdio_destroy(cdio);
		throw std::runtime_error("Unable to identify audio CD disc.");
	}

	AtScopeExit(cdio, drv)
	{
		cdio_cddap_close_no_free_cdio(drv);
		cdio_destroy(cdio);
	};

	cdio_cddap_verbose_set(drv, CDDA_MESSAGE_FORGETIT, CDDA_MESSAGE_FORGETIT);

	if (0 != cdio_cddap_open(drv))
		throw std::runtime_error("Unable to open disc.");

	auto lastSector = cdio_cddap_disc_lastsector(drv);
	TrackOffsets trackOffsets;

	int leadOut = (int)lastSector + 1;

	trackOffsets.frameOffsets.push_back(leadOut);

	int numTracks = (int)cdio_cddap_tracks(drv);

	if (numTracks <= 0)
		throw std::runtime_error("Invalid number of tracks found");

	trackOffsets.firstTrackNumber = cdio_get_first_track_num(drv->p_cdio);

	for (int i = 0; i < numTracks; ++i)
	{
		auto frameOffset = cdio_cddap_track_firstsector(drv, i + trackOffsets.firstTrackNumber);

		trackOffsets.frameOffsets.push_back((int)frameOffset);
	}

	trackOffsets.leadIn = CDIO_PREGAP_SECTORS;
	trackOffsets.lastTrackNumber = numTracks + trackOffsets.firstTrackNumber - 1;

	return trackOffsets;
}

static AllocatedPath
cdio_detect_device()
{
	char **devices = cdio_get_devices_with_cap(nullptr, CDIO_FS_AUDIO, false);

	if (devices == nullptr)
		return nullptr;

	AtScopeExit(devices) { cdio_free_device_list(devices); };

	if (devices[0] == nullptr)
		return nullptr;

	return AllocatedPath::FromFS(devices[0]);
}

struct CdioUri
{
	char device[64];
	int track;
};

static CdioUri
parse_cdio_uri (std::string_view src)
{
	CdioUri dest;

	src = StringAfterPrefixIgnoreCase(src, "cdda://"sv);

	const auto [device, track] = Split(src, '/');
	if (device.size() >= sizeof(dest.device))
		throw std::invalid_argument{"Device name is too long"};

	*std::copy(device.begin(), device.end(), dest.device) = '\0';

	if (!track.empty())
	{
		auto value = ParseInteger<uint_least16_t>(track);
		if (!value)
			throw std::invalid_argument{"Bad track number"};

		dest.track = *value;
	}
	else
		dest.track = -1;

	return dest;
}

int
main(int argc, char **argv) noexcept
try {
	if (argc != 2) {
		fprintf(stderr, "Usage: MusicbrainzCDID track-uri\n"
				"Where track-uri is a valid audio cd uri (starting with 'cdda://')\n"
				"(first track of default drive is 'cdda:///1')\n");
		return EXIT_FAILURE;
	}

	const std::string uri(argv[1]);
	const auto parsed_uri = parse_cdio_uri(uri);
	const AllocatedPath device = parsed_uri.device[0] != 0
		? AllocatedPath::FromFS(parsed_uri.device)
		: cdio_detect_device();

	if (device.IsNull())
		throw std::runtime_error("Unable find or access a CD-ROM drive with an audio CD in it.");

	auto trackOffsets = getTrackOffsetsFromDevice_cdio(device);

	if (!trackOffsets.isValid())
		throw std::runtime_error("Disc track offsets found are invalid.");

	auto musicbrainzId = makeMusicBrainzIdWithOffsets(trackOffsets);

	if (musicbrainzId == std::string())
		throw std::runtime_error("CDID creation failed");

	fprintf(stderr, "CDID found\n");
	fprintf(stdout, "%s\n", musicbrainzId.c_str());

	return EXIT_SUCCESS;

} catch (...) {
	PrintException(std::current_exception());
	return EXIT_FAILURE;
}
