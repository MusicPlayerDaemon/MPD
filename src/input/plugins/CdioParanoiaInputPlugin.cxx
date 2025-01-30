// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

/**
 * CD-Audio handling (requires libcdio_paranoia)
 */

#include "CdioParanoiaInputPlugin.hxx"
#include "lib/cdio/Paranoia.hxx"
#include "lib/fmt/RuntimeError.hxx"
#include "../InputStream.hxx"
#include "../InputPlugin.hxx"
#include "util/StringCompare.hxx"
#include "util/Domain.hxx"
#include "util/ByteOrder.hxx"
#include "util/NumberParser.hxx"
#include "util/ScopeExit.hxx"
#include "util/StringSplit.hxx"
#include "fs/AllocatedPath.hxx"
#include "Log.hxx"
#include "config/Block.hxx"

#include <algorithm>
#include <cassert>
#include <cstdint>

#include <cdio/cd_types.h>

using std::string_view_literals::operator""sv;

static constexpr Domain cdio_domain("cdio");

static bool default_reverse_endian;
static unsigned speed = 0;

/* Default to full paranoia, but allow skipping sectors. */
static int mode_flags = PARANOIA_MODE_FULL^PARANOIA_MODE_NEVERSKIP;

class CdioParanoiaInputStream final : public InputStream {
	cdrom_drive_t *const drv;
	CdIo_t *const cdio;
	CdromParanoia para;

	const lsn_t lsn_from;

	char buffer[CDIO_CD_FRAMESIZE_RAW];
	lsn_t buffer_lsn;

 public:
	CdioParanoiaInputStream(std::string_view _uri, Mutex &_mutex,
				cdrom_drive_t *_drv, CdIo_t *_cdio,
				bool reverse_endian,
				lsn_t _lsn_from, lsn_t lsn_to)
		:InputStream(_uri, _mutex),
		 drv(_drv), cdio(_cdio), para(drv),
		 lsn_from(_lsn_from),
		 buffer_lsn(-1)
	{
		para.SetMode(mode_flags);

		/* seek to beginning of the track */
		para.Seek(lsn_from);

		seekable = true;
		size = (lsn_to - lsn_from + 1) * CDIO_CD_FRAMESIZE_RAW;

		/* hack to make MPD select the "pcm" decoder plugin */
		SetMimeType(reverse_endian
			    ? "audio/x-mpd-cdda-pcm-reverse"
			    : "audio/x-mpd-cdda-pcm");
		SetReady();
	}

	~CdioParanoiaInputStream() override {
		para = {};
		cdio_cddap_close_no_free_cdio(drv);
		cdio_destroy(cdio);
	}

	CdioParanoiaInputStream(const CdioParanoiaInputStream &) = delete;
	CdioParanoiaInputStream &operator=(const CdioParanoiaInputStream &) = delete;

	/* virtual methods from InputStream */
	[[nodiscard]] bool IsEOF() const noexcept override;
	size_t Read(std::unique_lock<Mutex> &lock,
		    std::span<std::byte> dest) override;
	void Seek(std::unique_lock<Mutex> &lock, offset_type offset) override;
};

static void
input_cdio_init(EventLoop &, const ConfigBlock &block)
{
	const char *value = block.GetBlockValue("default_byte_order");
	if (value != nullptr) {
		if (strcmp(value, "little_endian") == 0)
			default_reverse_endian = IsBigEndian();
		else if (strcmp(value, "big_endian") == 0)
			default_reverse_endian = IsLittleEndian();
		else
			throw FmtRuntimeError("Unrecognized 'default_byte_order' setting: {}",
					      value);
	}
	speed = block.GetBlockValue("speed",0U);

	if (const auto *param = block.GetBlockParam("mode")) {
		param->With([](const char *s){
			if (StringIsEqual(s, "disable"))
				mode_flags = PARANOIA_MODE_DISABLE;
			else if (StringIsEqual(s, "overlap"))
				mode_flags = PARANOIA_MODE_OVERLAP;
			else if (StringIsEqual(s, "full"))
				mode_flags = PARANOIA_MODE_FULL;
			else
				throw std::invalid_argument{"Invalid paranoia mode"};
		});
	}

	if (const auto *param = block.GetBlockParam("skip")) {
		if (param->GetBoolValue())
			mode_flags &= ~PARANOIA_MODE_NEVERSKIP;
		else
			mode_flags |= PARANOIA_MODE_NEVERSKIP;
	}
}

struct CdioUri {
	char device[64];
	int track;
};

static CdioUri
parse_cdio_uri(std::string_view src)
{
	CdioUri dest;

	src = StringAfterPrefixIgnoreCase(src, "cdda://"sv);

	const auto [device, track] = Split(src, '/');
	if (device.size() >= sizeof(dest.device))
		throw std::invalid_argument{"Device name is too long"};

	*std::copy(device.begin(), device.end(), dest.device) = '\0';

	if (!track.empty()) {
		auto value = ParseInteger<uint_least16_t>(track);
		if (!value)
			throw std::invalid_argument{"Bad track number"};

		dest.track = *value;
	} else
		/* play the whole CD */
		dest.track = -1;

	return dest;
}

static AllocatedPath
cdio_detect_device()
{
	char **devices = cdio_get_devices_with_cap(nullptr, CDIO_FS_AUDIO,
						   false);
	if (devices == nullptr)
		return nullptr;

	AtScopeExit(devices) { cdio_free_device_list(devices); };

	if (devices[0] == nullptr)
		return nullptr;

	return AllocatedPath::FromFS(devices[0]);
}

static InputStreamPtr
input_cdio_open(std::string_view uri,
		Mutex &mutex)
{
	const auto parsed_uri = parse_cdio_uri(uri);

	/* get list of CD's supporting CD-DA */
	const AllocatedPath device = parsed_uri.device[0] != 0
		? AllocatedPath::FromFS(parsed_uri.device)
		: cdio_detect_device();
	if (device.IsNull())
		throw std::runtime_error("Unable find or access a CD-ROM drive with an audio CD in it.");

	/* Found such a CD-ROM with a CD-DA loaded. Use the first drive in the list. */
	const auto cdio = cdio_open(device.c_str(), DRIVER_UNKNOWN);
	if (cdio == nullptr)
		throw std::runtime_error("Failed to open CD drive");

	const auto drv = cdio_cddap_identify_cdio(cdio, 1, nullptr);
	if (drv == nullptr) {
		cdio_destroy(cdio);
		throw std::runtime_error("Unable to identify audio CD disc.");
	}

	cdio_cddap_verbose_set(drv, CDDA_MESSAGE_FORGETIT, CDDA_MESSAGE_FORGETIT);

	if (0 != cdio_cddap_open(drv)) {
		cdio_cddap_close_no_free_cdio(drv);
		cdio_destroy(cdio);
		throw std::runtime_error("Unable to open disc.");
	}

	if (speed > 0) {
		FmtDebug(cdio_domain, "Attempting to set CD speed to {}x",
			 speed);
		/* Negative value indicate error (e.g. -405: not supported) */
		if (cdio_cddap_speed_set(drv,speed) < 0)
			FmtDebug(cdio_domain, "Failed to set CD speed to {}x",
				 speed);
	}

	bool reverse_endian;
	const int be = data_bigendianp(drv);
	switch (be) {
	case -1:
		LogDebug(cdio_domain, "drive returns unknown audio data");
		reverse_endian = default_reverse_endian;
		break;

	case 0:
		LogDebug(cdio_domain, "drive returns audio data Little Endian");
		reverse_endian = IsBigEndian();
		break;

	case 1:
		LogDebug(cdio_domain, "drive returns audio data Big Endian");
		reverse_endian = IsLittleEndian();
		break;

	default:
		cdio_cddap_close_no_free_cdio(drv);
		cdio_destroy(cdio);
		throw FmtRuntimeError("Drive returns unknown data type {}",
				      be);
	}

	lsn_t lsn_from, lsn_to;
	if (parsed_uri.track >= 0) {
		lsn_from = cdio_cddap_track_firstsector(drv, parsed_uri.track);
		lsn_to = cdio_cddap_track_lastsector(drv, parsed_uri.track);
	} else {
		lsn_from = cdio_cddap_disc_firstsector(drv);
		lsn_to = cdio_cddap_disc_lastsector(drv);
	}

	/* LSNs < 0 indicate errors (e.g. -401: Invaid track, -402: no pregap) */
	if(lsn_from < 0 || lsn_to < 0)
		throw FmtRuntimeError("Error {} on track {}",
				      lsn_from < 0 ? lsn_from : lsn_to, parsed_uri.track);

	/* Only check for audio track if not pregap or whole CD */
	if (!cdio_cddap_track_audiop(drv, parsed_uri.track) && parsed_uri.track > 0)
		throw FmtRuntimeError("No audio track: {}",
				      parsed_uri.track);

	return std::make_unique<CdioParanoiaInputStream>(uri, mutex,
							 drv, cdio,
							 reverse_endian,
							 lsn_from, lsn_to);
}

void
CdioParanoiaInputStream::Seek(std::unique_lock<Mutex> &,
			      offset_type new_offset)
{
	if (new_offset > size)
		throw FmtRuntimeError("Invalid offset to seek {} ({})",
				      new_offset, size);

	/* simple case */
	if (new_offset == offset)
		return;

	/* calculate current LSN */
	const lsn_t lsn_relofs = new_offset / CDIO_CD_FRAMESIZE_RAW;

	if (lsn_relofs != buffer_lsn) {
		const ScopeUnlock unlock(mutex);
		para.Seek(lsn_from + lsn_relofs);
	}

	offset = new_offset;
}

size_t
CdioParanoiaInputStream::Read(std::unique_lock<Mutex> &,
			      std::span<std::byte> dest)
{
	/* end of track ? */
	if (IsEOF())
		return 0;

	//current sector was changed ?
	const int16_t *rbuf;

	const lsn_t lsn_relofs = offset / CDIO_CD_FRAMESIZE_RAW;
	const std::size_t diff = offset % CDIO_CD_FRAMESIZE_RAW;

	if (lsn_relofs != buffer_lsn) {
		const ScopeUnlock unlock(mutex);

		try {
			rbuf = para.Read().data();
		} catch (...) {
			char *s_err = cdio_cddap_errors(drv);
			if (s_err) {
				FmtError(cdio_domain,
					 "paranoia_read: {}", s_err);
				cdio_cddap_free_messages(s_err);
			}

			throw;
		}

		//store current buffer
		memcpy(buffer, rbuf, CDIO_CD_FRAMESIZE_RAW);
		buffer_lsn = lsn_relofs;
	} else {
		//use cached sector
		rbuf = (const int16_t *)buffer;
	}

	const size_t maxwrite = CDIO_CD_FRAMESIZE_RAW - diff;  //# of bytes pending in current buffer
	const std::size_t nbytes = std::min(dest.size(), maxwrite);

	//skip diff bytes from this lsn
	memcpy(dest.data(), ((const char *)rbuf) + diff, nbytes);

	//update offset
	offset += nbytes;

	return nbytes;
}

bool
CdioParanoiaInputStream::IsEOF() const noexcept
{
	return offset >= size;
}

static constexpr const char *cdio_paranoia_prefixes[] = {
	"cdda://",
	nullptr
};

const InputPlugin input_plugin_cdio_paranoia = {
	"cdio_paranoia",
	cdio_paranoia_prefixes,
	input_cdio_init,
	nullptr,
	input_cdio_open,
	nullptr
};
