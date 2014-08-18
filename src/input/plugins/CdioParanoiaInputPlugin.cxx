/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
 * http://www.musicpd.org
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

/**
 * CD-Audio handling (requires libcdio_paranoia)
 */

#include "config.h"
#include "CdioParanoiaInputPlugin.hxx"
#include "../InputStream.hxx"
#include "../InputPlugin.hxx"
#include "util/StringUtil.hxx"
#include "util/Error.hxx"
#include "util/Domain.hxx"
#include "system/ByteOrder.hxx"
#include "fs/AllocatedPath.hxx"
#include "Log.hxx"
#include "config/ConfigData.hxx"
#include "config/ConfigError.hxx"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <glib.h>
#include <assert.h>

#ifdef HAVE_CDIO_PARANOIA_PARANOIA_H
#include <cdio/paranoia/paranoia.h>
#else
#include <cdio/paranoia.h>
#endif

#include <cdio/cd_types.h>

class CdioParanoiaInputStream final : public InputStream {
	cdrom_drive_t *const drv;
	CdIo_t *const cdio;
	cdrom_paranoia_t *const para;

	const lsn_t lsn_from, lsn_to;
	int lsn_relofs;

	char buffer[CDIO_CD_FRAMESIZE_RAW];
	int buffer_lsn;

 public:
	CdioParanoiaInputStream(const char *_uri, Mutex &_mutex, Cond &_cond,
				cdrom_drive_t *_drv, CdIo_t *_cdio,
				bool reverse_endian,
				lsn_t _lsn_from, lsn_t _lsn_to)
		:InputStream(_uri, _mutex, _cond),
		 drv(_drv), cdio(_cdio), para(cdio_paranoia_init(drv)),
		 lsn_from(_lsn_from), lsn_to(_lsn_to),
		 lsn_relofs(0),
		 buffer_lsn(-1)
	{
		/* Set reading mode for full paranoia, but allow
		   skipping sectors. */
		paranoia_modeset(para,
				 PARANOIA_MODE_FULL^PARANOIA_MODE_NEVERSKIP);

		/* seek to beginning of the track */
		cdio_paranoia_seek(para, lsn_from, SEEK_SET);

		seekable = true;
		size = (lsn_to - lsn_from + 1) * CDIO_CD_FRAMESIZE_RAW;

		/* hack to make MPD select the "pcm" decoder plugin */
		SetMimeType(reverse_endian
			    ? "audio/x-mpd-cdda-pcm-reverse"
			    : "audio/x-mpd-cdda-pcm");
		SetReady();
	}

	~CdioParanoiaInputStream() {
		cdio_paranoia_free(para);
		cdio_cddap_close_no_free_cdio(drv);
		cdio_destroy(cdio);
	}

	/* virtual methods from InputStream */
	bool IsEOF() override;
	size_t Read(void *ptr, size_t size, Error &error) override;
	bool Seek(offset_type offset, Error &error) override;
};

static constexpr Domain cdio_domain("cdio");

static bool default_reverse_endian;

static InputPlugin::InitResult
input_cdio_init(const config_param &param, Error &error)
{
	const char *value = param.GetBlockValue("default_byte_order");
	if (value != nullptr) {
		if (strcmp(value, "little_endian") == 0)
			default_reverse_endian = IsBigEndian();
		else if (strcmp(value, "big_endian") == 0)
			default_reverse_endian = IsLittleEndian();
		else {
			error.Format(config_domain, 0,
				     "Unrecognized 'default_byte_order' setting: %s",
				     value);
			return InputPlugin::InitResult::ERROR;
		}
	}

	return InputPlugin::InitResult::SUCCESS;
}

struct cdio_uri {
	char device[64];
	int track;
};

static bool
parse_cdio_uri(struct cdio_uri *dest, const char *src, Error &error)
{
	if (!StringStartsWith(src, "cdda://"))
		return false;

	src += 7;

	if (*src == 0) {
		/* play the whole CD in the default drive */
		dest->device[0] = 0;
		dest->track = -1;
		return true;
	}

	const char *slash = strrchr(src, '/');
	if (slash == nullptr) {
		/* play the whole CD in the specified drive */
		g_strlcpy(dest->device, src, sizeof(dest->device));
		dest->track = -1;
		return true;
	}

	size_t device_length = slash - src;
	if (device_length >= sizeof(dest->device))
		device_length = sizeof(dest->device) - 1;

	memcpy(dest->device, src, device_length);
	dest->device[device_length] = 0;

	const char *track = slash + 1;

	char *endptr;
	dest->track = strtoul(track, &endptr, 10);
	if (*endptr != 0) {
		error.Set(cdio_domain, "Malformed track number");
		return false;
	}

	if (endptr == track)
		/* play the whole CD */
		dest->track = -1;

	return true;
}

static AllocatedPath
cdio_detect_device(void)
{
	char **devices = cdio_get_devices_with_cap(nullptr, CDIO_FS_AUDIO,
						   false);
	if (devices == nullptr)
		return AllocatedPath::Null();

	AllocatedPath path = AllocatedPath::FromFS(devices[0]);
	cdio_free_device_list(devices);
	return path;
}

static InputStream *
input_cdio_open(const char *uri,
		Mutex &mutex, Cond &cond,
		Error &error)
{
	struct cdio_uri parsed_uri;
	if (!parse_cdio_uri(&parsed_uri, uri, error))
		return nullptr;

	/* get list of CD's supporting CD-DA */
	const AllocatedPath device = parsed_uri.device[0] != 0
		? AllocatedPath::FromFS(parsed_uri.device)
		: cdio_detect_device();
	if (device.IsNull()) {
		error.Set(cdio_domain,
			  "Unable find or access a CD-ROM drive with an audio CD in it.");
		return nullptr;
	}

	/* Found such a CD-ROM with a CD-DA loaded. Use the first drive in the list. */
	const auto cdio = cdio_open(device.c_str(), DRIVER_UNKNOWN);
	if (cdio == nullptr) {
		error.Set(cdio_domain, "Failed to open CD drive");
		return nullptr;
	}

	const auto drv = cdio_cddap_identify_cdio(cdio, 1, nullptr);
	if (drv == nullptr) {
		error.Set(cdio_domain, "Unable to identify audio CD disc.");
		cdio_destroy(cdio);
		return nullptr;
	}

	cdda_verbose_set(drv, CDDA_MESSAGE_FORGETIT, CDDA_MESSAGE_FORGETIT);

	if (0 != cdio_cddap_open(drv)) {
		cdio_cddap_close_no_free_cdio(drv);
		cdio_destroy(cdio);
		error.Set(cdio_domain, "Unable to open disc.");
		return nullptr;
	}

	bool reverse_endian;
	switch (data_bigendianp(drv)) {
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
		error.Format(cdio_domain, "Drive returns unknown data type %d",
			     data_bigendianp(drv));
		cdio_cddap_close_no_free_cdio(drv);
		cdio_destroy(cdio);
		return nullptr;
	}

	lsn_t lsn_from, lsn_to;
	if (parsed_uri.track >= 0) {
		lsn_from = cdio_get_track_lsn(cdio, parsed_uri.track);
		lsn_to = cdio_get_track_last_lsn(cdio, parsed_uri.track);
	} else {
		lsn_from = 0;
		lsn_to = cdio_get_disc_last_lsn(cdio);
	}

	return new CdioParanoiaInputStream(uri, mutex, cond,
					   drv, cdio, reverse_endian,
					   lsn_from, lsn_to);
}

bool
CdioParanoiaInputStream::Seek(offset_type new_offset, Error &error)
{
	if (new_offset > size) {
		error.Format(cdio_domain, "Invalid offset to seek %ld (%ld)",
			     (long int)new_offset, (long int)size);
		return false;
	}

	/* simple case */
	if (new_offset == offset)
		return true;

	/* calculate current LSN */
	lsn_relofs = new_offset / CDIO_CD_FRAMESIZE_RAW;
	offset = new_offset;

	cdio_paranoia_seek(para, lsn_from + lsn_relofs, SEEK_SET);

	return true;
}

size_t
CdioParanoiaInputStream::Read(void *ptr, size_t length, Error &error)
{
	size_t nbytes = 0;
	int diff;
	size_t len, maxwrite;
	int16_t *rbuf;
	char *s_err, *s_mess;
	char *wptr = (char *) ptr;

	while (length > 0) {


		/* end of track ? */
		if (lsn_from + lsn_relofs > lsn_to)
			break;

		//current sector was changed ?
		if (lsn_relofs != buffer_lsn) {
			rbuf = cdio_paranoia_read(para, nullptr);

			s_err = cdda_errors(drv);
			if (s_err) {
				FormatError(cdio_domain,
					    "paranoia_read: %s", s_err);
				free(s_err);
			}
			s_mess = cdda_messages(drv);
			if (s_mess) {
				free(s_mess);
			}
			if (!rbuf) {
				error.Set(cdio_domain,
					  "paranoia read error. Stopping.");
				return 0;
			}
			//store current buffer
			memcpy(buffer, rbuf, CDIO_CD_FRAMESIZE_RAW);
			buffer_lsn = lsn_relofs;
		} else {
			//use cached sector
			rbuf = (int16_t *)buffer;
		}

		//correct offset
		diff = offset - lsn_relofs * CDIO_CD_FRAMESIZE_RAW;

		assert(diff >= 0 && diff < CDIO_CD_FRAMESIZE_RAW);

		maxwrite = CDIO_CD_FRAMESIZE_RAW - diff;  //# of bytes pending in current buffer
		len = (length < maxwrite? length : maxwrite);

		//skip diff bytes from this lsn
		memcpy(wptr, ((char*)rbuf) + diff, len);
		//update pointer
		wptr += len;
		nbytes += len;

		//update offset
		offset += len;
		lsn_relofs = offset / CDIO_CD_FRAMESIZE_RAW;
		//update length
		length -= len;
	}

	return nbytes;
}

bool
CdioParanoiaInputStream::IsEOF()
{
	return lsn_from + lsn_relofs > lsn_to;
}

const InputPlugin input_plugin_cdio_paranoia = {
	"cdio_paranoia",
	input_cdio_init,
	nullptr,
	input_cdio_open,
};
