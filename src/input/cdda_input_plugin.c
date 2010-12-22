/*
 * Copyright (C) 2003-2010 The Music Player Daemon Project
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
#include "input/cdda_input_plugin.h"
#include "input_plugin.h"
#include "refcount.h"
#include "pcm_buffer.h"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <glib.h>
#include <assert.h>

#include <cdio/paranoia.h>
#include <cdio/cd_types.h>

struct input_cdda {
	struct input_stream base;

	cdrom_drive_t *drv;
	CdIo_t *cdio;
	cdrom_paranoia_t *para;

	int endian;

	lsn_t lsn_from, lsn_to;
	int lsn_relofs;

	int trackno;

	char buffer[CDIO_CD_FRAMESIZE_RAW];
	int buffer_lsn;

	struct pcm_buffer conv_buffer;
};

static inline GQuark
cdda_quark(void)
{
	return g_quark_from_static_string("cdda");
}

static void
input_cdda_close(struct input_stream *is)
{
	struct input_cdda *i = (struct input_cdda *)is;

	pcm_buffer_deinit(&i->conv_buffer);

	if (i->para)
		cdio_paranoia_free(i->para);
	if (i->drv)
		cdio_cddap_close_no_free_cdio( i->drv);
	if (i->cdio)
		cdio_destroy( i->cdio );

	input_stream_deinit(&i->base);
	g_free(i);
}

static char *
cdda_detect_device(void)
{
	char **devices = cdio_get_devices_with_cap(NULL, CDIO_FS_AUDIO, false);
	if (devices == NULL)
		return NULL;

	char *device = g_strdup(devices[0]);
	cdio_free_device_list(devices);

	return device;
}

static struct input_stream *
input_cdda_open(const char *uri, GError **error_r)
{
	struct input_cdda *i;

	if (!g_str_has_prefix(uri, "cdda://"))
		return NULL;

	i = g_new(struct input_cdda, 1);
	input_stream_init(&i->base, &input_plugin_cdda, uri);

	/* initialize everything (should be already) */
	i->drv = NULL;
	i->cdio = NULL;
	i->para = NULL;
	pcm_buffer_init(&i->conv_buffer);

	if (parse_cdda_uri(uri, &drive, &trackno) == -1) {
		g_set_error(error_r, cdda_quark(), 0,
			    "Unable parse URI\n");
		input_cdda_close(&i->base);
		return NULL;
	}

	/* get list of CD's supporting CD-DA */
	char *device = cdda_detect_device();
	if (device == NULL) {
		g_set_error(error_r, cdda_quark(), 0,
			    "Unable find or access a CD-ROM drive with an audio CD in it.");
		input_cdda_close(&i->base);
		return NULL;
	}

	/* Found such a CD-ROM with a CD-DA loaded. Use the first drive in the list. */
	i->cdio = cdio_open(device, DRIVER_UNKNOWN);
	g_free(device);

	i->drv = cdio_cddap_identify_cdio(i->cdio, 1, NULL);

	if ( !i->drv ) {
		g_set_error(error_r, cdda_quark(), 0,
			    "Unable to identify audio CD disc.");
		input_cdda_close(&i->base);
		return NULL;
	}

	cdda_verbose_set(i->drv, CDDA_MESSAGE_FORGETIT, CDDA_MESSAGE_FORGETIT);

	if ( 0 != cdio_cddap_open(i->drv) ) {
		g_set_error(error_r, cdda_quark(), 0, "Unable to open disc.");
		input_cdda_close(&i->base);
		return NULL;
	}

	i->endian = data_bigendianp(i->drv);
	switch (i->endian) {
	case -1:
		g_debug("cdda: drive returns unknown audio data, assuming Little Endian");
		i->endian = 0;
		break;
	case 0:
		g_debug("cdda: drive returns audio data Little Endian.");
		break;
	case 1:
		g_debug("cdda: drive returns audio data Big Endian.");
		break;
	default:
		g_set_error(error_r, cdda_quark(), 0,
			    "Drive returns unknown data type %d", i->endian);
		input_cdda_close(i)
		return NULL;
	}

	if (i->trackno == -1) {
		g_set_error(error_r, cdda_quark(), 0,
			    "Invalid track # in %s", uri);
		input_cdda_close(&i->base);
		return NULL;
	}


	i->lsn_relofs = 0;
	i->lsn_from = cdio_get_track_lsn( i->cdio, i->trackno );
	i->lsn_to = cdio_get_track_last_lsn( i->cdio, i->trackno );

	i->para = cdio_paranoia_init(i->drv);

	/* Set reading mode for full paranoia, but allow skipping sectors. */
	paranoia_modeset(i->para, PARANOIA_MODE_FULL^PARANOIA_MODE_NEVERSKIP);

	/* seek to beginning of the track */
	cdio_paranoia_seek(i->para, i->lsn_from, SEEK_SET);

	i->base.ready = true;
	i->base.seekable = true;
	i->base.size = (i->lsn_to - i->lsn_from + 1) * CDIO_CD_FRAMESIZE_RAW;

	/* hack to make MPD select the "pcm" decoder plugin */
	i->base.mime = g_strdup("audio/x-mpd-cdda-pcm");

	return &i->base;
}


/* single archive handling */
static int
input_cdda_archive_extract_trackno(const char *path)
{
	long value;
	char *endptr, *str;

	//remove .wav
	str = strrchr(path, '.');
	if (str)
		*str = 0;

	//remove leading 0's
	while (*path == '0')
		path++;

	value = strtol(path, &endptr, 0);
	if (*endptr != 0 || value < 0) {
		return -1;
	}
	return value;
}

static bool
input_cdda_seek(struct input_stream *is,
		goffset offset, int whence, GError **error_r)
{
	struct input_cdda *cis = (struct input_cdda *)is;

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
		g_set_error(error_r, cdda_quark(), 0,
			    "Invalid offset to seek %ld (%ld)",
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
input_cdda_read(struct input_stream *is, void *ptr, size_t length,
		GError **error_r)
{
	struct input_cdda *cis = (struct input_cdda *)is;
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
			rbuf = cdio_paranoia_read(cis->para, NULL);

			s_err = cdda_errors(cis->drv);
			if (s_err) {
				g_warning("paranoia_read: %s", s_err );
				free(s_err);
			}
			s_mess = cdda_messages(cis->drv);
			if (s_mess) {
				free(s_mess);
			}
			if (!rbuf) {
				g_set_error(error_r, cdda_quark(), 0,
					"paranoia read error. Stopping.");
				return 0;
			}
			//do the swapping if nessesary
			if (cis->endian != 0) {
				uint16_t *conv_buffer = pcm_buffer_get(&cis->conv_buffer, CDIO_CD_FRAMESIZE_RAW );
				/* do endian conversion ! */
				pcm16_to_wave( conv_buffer, (uint16_t*) rbuf, CDIO_CD_FRAMESIZE_RAW);
				rbuf = (int16_t *)conv_buffer;
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
input_cdda_eof(struct input_stream *is)
{
	struct input_cdda *cis = (struct input_cdda *)is;

	return (cis->lsn_from + cis->lsn_relofs > cis->lsn_to);
}

const struct input_plugin input_plugin_cdda = {
	.open = input_cdda_open,
	.close = input_cdda_close,
	.seek = input_cdda_seek,
	.read = input_cdda_read,
	.eof = input_cdda_eof
};
