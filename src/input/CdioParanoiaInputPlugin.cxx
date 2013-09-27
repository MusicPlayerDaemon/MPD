/*
 * Copyright (C) 2003-2013 The Music Player Daemon Project
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
#include "InputInternal.hxx"
#include "InputStream.hxx"
#include "InputPlugin.hxx"
#include "util/Error.hxx"
#include "util/Domain.hxx"
#include "Log.hxx"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <glib.h>
#include <assert.h>

#include <cdio/paranoia.h>
#include <cdio/cd_types.h>

struct CdioParanoiaInputStream {
	struct input_stream base;

	cdrom_drive_t *drv;
	CdIo_t *cdio;
	cdrom_paranoia_t *para;

	lsn_t lsn_from, lsn_to;
	int lsn_relofs;

	int trackno;

	char buffer[CDIO_CD_FRAMESIZE_RAW];
	int buffer_lsn;

	CdioParanoiaInputStream(const char *uri, Mutex &mutex, Cond &cond,
				int _trackno)
		:base(input_plugin_cdio_paranoia, uri, mutex, cond),
		 drv(nullptr), cdio(nullptr), para(nullptr),
		 trackno(_trackno)
	{
	}

	~CdioParanoiaInputStream() {
		if (para != nullptr)
			cdio_paranoia_free(para);
		if (drv != nullptr)
			cdio_cddap_close_no_free_cdio(drv);
		if (cdio != nullptr)
			cdio_destroy(cdio);
	}
};

static constexpr Domain cdio_domain("cdio");

static void
input_cdio_close(struct input_stream *is)
{
	CdioParanoiaInputStream *i = (CdioParanoiaInputStream *)is;

	delete i;
}

struct cdio_uri {
	char device[64];
	int track;
};

static bool
parse_cdio_uri(struct cdio_uri *dest, const char *src, Error &error)
{
	if (!g_str_has_prefix(src, "cdda://"))
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

static char *
cdio_detect_device(void)
{
	char **devices = cdio_get_devices_with_cap(nullptr, CDIO_FS_AUDIO,
						   false);
	if (devices == nullptr)
		return nullptr;

	char *device = g_strdup(devices[0]);
	cdio_free_device_list(devices);

	return device;
}

static struct input_stream *
input_cdio_open(const char *uri,
		Mutex &mutex, Cond &cond,
		Error &error)
{
	struct cdio_uri parsed_uri;
	if (!parse_cdio_uri(&parsed_uri, uri, error))
		return nullptr;

	CdioParanoiaInputStream *i =
		new CdioParanoiaInputStream(uri, mutex, cond,
					    parsed_uri.track);

	/* get list of CD's supporting CD-DA */
	char *device = parsed_uri.device[0] != 0
		? g_strdup(parsed_uri.device)
		: cdio_detect_device();
	if (device == nullptr) {
		error.Set(cdio_domain,
			  "Unable find or access a CD-ROM drive with an audio CD in it.");
		delete i;
		return nullptr;
	}

	/* Found such a CD-ROM with a CD-DA loaded. Use the first drive in the list. */
	i->cdio = cdio_open(device, DRIVER_UNKNOWN);
	g_free(device);

	i->drv = cdio_cddap_identify_cdio(i->cdio, 1, nullptr);

	if ( !i->drv ) {
		error.Set(cdio_domain, "Unable to identify audio CD disc.");
		delete i;
		return nullptr;
	}

	cdda_verbose_set(i->drv, CDDA_MESSAGE_FORGETIT, CDDA_MESSAGE_FORGETIT);

	if ( 0 != cdio_cddap_open(i->drv) ) {
		error.Set(cdio_domain, "Unable to open disc.");
		delete i;
		return nullptr;
	}

	bool reverse_endian;
	switch (data_bigendianp(i->drv)) {
	case -1:
		LogDebug(cdio_domain, "drive returns unknown audio data");
		reverse_endian = false;
		break;

	case 0:
		LogDebug(cdio_domain, "drive returns audio data Little Endian");
		reverse_endian = G_BYTE_ORDER == G_BIG_ENDIAN;
		break;

	case 1:
		LogDebug(cdio_domain, "drive returns audio data Big Endian");
		reverse_endian = G_BYTE_ORDER == G_LITTLE_ENDIAN;
		break;

	default:
		error.Format(cdio_domain, "Drive returns unknown data type %d",
			     data_bigendianp(i->drv));
		delete i;
		return nullptr;
	}

	i->lsn_relofs = 0;

	if (i->trackno >= 0) {
		i->lsn_from = cdio_get_track_lsn(i->cdio, i->trackno);
		i->lsn_to = cdio_get_track_last_lsn(i->cdio, i->trackno);
	} else {
		i->lsn_from = 0;
		i->lsn_to = cdio_get_disc_last_lsn(i->cdio);
	}

	i->para = cdio_paranoia_init(i->drv);

	/* Set reading mode for full paranoia, but allow skipping sectors. */
	paranoia_modeset(i->para, PARANOIA_MODE_FULL^PARANOIA_MODE_NEVERSKIP);

	/* seek to beginning of the track */
	cdio_paranoia_seek(i->para, i->lsn_from, SEEK_SET);

	i->base.ready = true;
	i->base.seekable = true;
	i->base.size = (i->lsn_to - i->lsn_from + 1) * CDIO_CD_FRAMESIZE_RAW;

	/* hack to make MPD select the "pcm" decoder plugin */
	i->base.mime = g_strdup(reverse_endian
				? "audio/x-mpd-cdda-pcm-reverse"
				: "audio/x-mpd-cdda-pcm");

	return &i->base;
}

static bool
input_cdio_seek(struct input_stream *is,
		goffset offset, int whence, Error &error)
{
	CdioParanoiaInputStream *cis = (CdioParanoiaInputStream *)is;

	/* calculate absolute offset */
	switch (whence) {
	case SEEK_SET:
		break;
	case SEEK_CUR:
		offset += cis->base.offset;
		break;
	case SEEK_END:
		offset += cis->base.size;
		break;
	}

	if (offset < 0 || offset > cis->base.size) {
		error.Format(cdio_domain, "Invalid offset to seek %ld (%ld)",
			     (long int)offset, (long int)cis->base.size);
		return false;
	}

	/* simple case */
	if (offset == cis->base.offset)
		return true;

	/* calculate current LSN */
	cis->lsn_relofs = offset / CDIO_CD_FRAMESIZE_RAW;
	cis->base.offset = offset;

	cdio_paranoia_seek(cis->para, cis->lsn_from + cis->lsn_relofs, SEEK_SET);

	return true;
}

static size_t
input_cdio_read(struct input_stream *is, void *ptr, size_t length,
		Error &error)
{
	CdioParanoiaInputStream *cis = (CdioParanoiaInputStream *)is;
	size_t nbytes = 0;
	int diff;
	size_t len, maxwrite;
	int16_t *rbuf;
	char *s_err, *s_mess;
	char *wptr = (char *) ptr;

	while (length > 0) {


		/* end of track ? */
		if (cis->lsn_from + cis->lsn_relofs > cis->lsn_to)
			break;

		//current sector was changed ?
		if (cis->lsn_relofs != cis->buffer_lsn) {
			rbuf = cdio_paranoia_read(cis->para, nullptr);

			s_err = cdda_errors(cis->drv);
			if (s_err) {
				FormatError(cdio_domain,
					    "paranoia_read: %s", s_err);
				free(s_err);
			}
			s_mess = cdda_messages(cis->drv);
			if (s_mess) {
				free(s_mess);
			}
			if (!rbuf) {
				error.Set(cdio_domain,
					  "paranoia read error. Stopping.");
				return 0;
			}
			//store current buffer
			memcpy(cis->buffer, rbuf, CDIO_CD_FRAMESIZE_RAW);
			cis->buffer_lsn = cis->lsn_relofs;
		} else {
			//use cached sector
			rbuf = (int16_t*) cis->buffer;
		}

		//correct offset
		diff = cis->base.offset - cis->lsn_relofs * CDIO_CD_FRAMESIZE_RAW;

		assert(diff >= 0 && diff < CDIO_CD_FRAMESIZE_RAW);

		maxwrite = CDIO_CD_FRAMESIZE_RAW - diff;  //# of bytes pending in current buffer
		len = (length < maxwrite? length : maxwrite);

		//skip diff bytes from this lsn
		memcpy(wptr, ((char*)rbuf) + diff, len);
		//update pointer
		wptr += len;
		nbytes += len;

		//update offset
		cis->base.offset += len;
		cis->lsn_relofs = cis->base.offset / CDIO_CD_FRAMESIZE_RAW;
		//update length
		length -= len;
	}

	return nbytes;
}

static bool
input_cdio_eof(struct input_stream *is)
{
	CdioParanoiaInputStream *cis = (CdioParanoiaInputStream *)is;

	return (cis->lsn_from + cis->lsn_relofs > cis->lsn_to);
}

const struct input_plugin input_plugin_cdio_paranoia = {
	"cdio_paranoia",
	nullptr,
	nullptr,
	input_cdio_open,
	input_cdio_close,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	input_cdio_read,
	input_cdio_eof,
	input_cdio_seek,
};
