/*
 * Copyright (C) 2003-2009 The Music Player Daemon Project
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

#include "config.h" /* must be first for large file support */
#include "decoder_api.h"
#include "audio_check.h"

#include <glib.h>

#include <mpg123.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "mpg123"

static bool
mpd_mpg123_init(G_GNUC_UNUSED const struct config_param *param)
{
	mpg123_init();

	return true;
}

static void
mpd_mpg123_finish(void)
{
	mpg123_exit();
}

/**
 * Opens a file with an existing #mpg123_handle.
 *
 * @param handle a handle which was created before; on error, this
 * function will not free it
 * @param audio_format this parameter is filled after successful
 * return
 * @return true on success
 */
static bool
mpd_mpg123_open(mpg123_handle *handle, const char *path_fs,
		struct audio_format *audio_format)
{
	GError *gerror = NULL;
	char *path_dup;
	int error;
	int channels, encoding;
	long rate;

	/* mpg123_open() wants a writable string :-( */
	path_dup = g_strdup(path_fs);

	error = mpg123_open(handle, path_dup);
	g_free(path_dup);
	if (error != MPG123_OK) {
		g_warning("libmpg123 failed to open %s: %s",
			  path_fs, mpg123_plain_strerror(error));
		return false;
	}

	/* obtain the audio format */

	error = mpg123_getformat(handle, &rate, &channels, &encoding);
	if (error != MPG123_OK) {
		g_warning("mpg123_getformat() failed: %s",
			  mpg123_plain_strerror(error));
		return false;
	}

	if (encoding != MPG123_ENC_SIGNED_16) {
		/* other formats not yet implemented */
		g_warning("expected MPG123_ENC_SIGNED_16, got %d", encoding);
		return false;
	}

	if (!audio_format_init_checked(audio_format, rate, 16,
				       channels, &gerror)) {
		g_warning("%s", gerror->message);
		g_error_free(gerror);
		return false;
	}

	return true;
}

static void
mpd_mpg123_file_decode(struct decoder *decoder, const char *path_fs)
{
	struct audio_format audio_format;
	mpg123_handle *handle;
	int error;
	off_t num_samples, position;
	enum decoder_command cmd;

	/* open the file */

	handle = mpg123_new(NULL, &error);
	if (handle == NULL) {
		g_warning("mpg123_new() failed: %s",
			  mpg123_plain_strerror(error));
		return;
	}

	if (!mpd_mpg123_open(handle, path_fs, &audio_format)) {
		mpg123_delete(handle);
		return;
	}

	num_samples = mpg123_length(handle);

	/* tell MPD core we're ready */

	decoder_initialized(decoder, &audio_format, false,
			    (float)num_samples /
			    (float)audio_format.sample_rate);

	/* the decoder main loop */

	do {
		unsigned char buffer[8192];
		size_t nbytes;

		position = mpg123_tell(handle);

		/* decode */

		error = mpg123_read(handle, buffer, sizeof(buffer), &nbytes);
		if (error != MPG123_OK) {
			if (error != MPG123_DONE)
				g_warning("mpg123_read() failed: %s",
					  mpg123_plain_strerror(error));
			break;
		}

		/* send to MPD */

		cmd = decoder_data(decoder, NULL, buffer, nbytes,
				   (float)position /
				   (float)audio_format.sample_rate,
				   0, NULL);

		/* seeking not yet implemented */
	} while (cmd == DECODE_COMMAND_NONE);

	/* cleanup */

	mpg123_delete(handle);
}

static struct tag *
mpd_mpg123_tag_dup(const char *path_fs)
{
	struct audio_format audio_format;
	mpg123_handle *handle;
	int error;
	off_t num_samples;
	struct tag *tag;

	handle = mpg123_new(NULL, &error);
	if (handle == NULL) {
		g_warning("mpg123_new() failed: %s",
			  mpg123_plain_strerror(error));
		return NULL;
	}

	if (!mpd_mpg123_open(handle, path_fs, &audio_format)) {
		mpg123_delete(handle);
		return NULL;
	}

	num_samples = mpg123_length(handle);
	if (num_samples <= 0) {
		mpg123_delete(handle);
		return NULL;
	}

	tag = tag_new();

	tag->time = num_samples / audio_format.sample_rate;

	/* ID3 tag support not yet implemented */

	mpg123_delete(handle);
	return tag;
}

static const char *const mpg123_suffixes[] = {
	"mp3",
	NULL
};

const struct decoder_plugin mpg123_decoder_plugin = {
	.name = "mpg123",
	.init = mpd_mpg123_init,
	.finish = mpd_mpg123_finish,
	.file_decode = mpd_mpg123_file_decode,
	/* streaming not yet implemented */
	.tag_dup = mpd_mpg123_tag_dup,
	.suffixes = mpg123_suffixes,
};
