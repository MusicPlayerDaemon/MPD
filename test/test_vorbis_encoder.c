/*
 * Copyright (C) 2003-2012 The Music Player Daemon Project
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

#include "config.h"
#include "encoder_list.h"
#include "encoder_plugin.h"
#include "audio_format.h"
#include "conf.h"
#include "stdbin.h"
#include "tag.h"

#include <glib.h>

#include <stddef.h>
#include <unistd.h>

static uint8_t zero[256];

static void
encoder_to_stdout(struct encoder *encoder)
{
	size_t length;
	static char buffer[32768];

	while ((length = encoder_read(encoder, buffer, sizeof(buffer))) > 0) {
		G_GNUC_UNUSED ssize_t ignored = write(1, buffer, length);
	}
}

int
main(G_GNUC_UNUSED int argc, G_GNUC_UNUSED char **argv)
{
	G_GNUC_UNUSED bool success;

	/* create the encoder */

	const struct encoder_plugin *plugin = encoder_plugin_get("vorbis");
	assert(plugin != NULL);

	struct config_param *param = config_new_param(NULL, -1);
	config_add_block_param(param, "quality", "5.0", -1);

	struct encoder *encoder = encoder_init(plugin, param, NULL);
	assert(encoder != NULL);

	/* open the encoder */

	struct audio_format audio_format;

	audio_format_init(&audio_format, 44100, SAMPLE_FORMAT_S16, 2);
	success = encoder_open(encoder, &audio_format, NULL);
	assert(success);

	encoder_to_stdout(encoder);

	/* write a block of data */

	success = encoder_write(encoder, zero, sizeof(zero), NULL);
	assert(success);

	encoder_to_stdout(encoder);

	/* write a tag */

	success = encoder_pre_tag(encoder, NULL);
	assert(success);

	encoder_to_stdout(encoder);

	struct tag *tag = tag_new();
	tag_add_item(tag, TAG_ARTIST, "Foo");
	tag_add_item(tag, TAG_TITLE, "Bar");

	success = encoder_tag(encoder, tag, NULL);
	assert(success);

	tag_free(tag);

	encoder_to_stdout(encoder);

	/* write another block of data */

	success = encoder_write(encoder, zero, sizeof(zero), NULL);
	assert(success);

	/* finish */

	success = encoder_end(encoder, NULL);
	assert(success);

	encoder_to_stdout(encoder);

	encoder_close(encoder);
	encoder_finish(encoder);
}
