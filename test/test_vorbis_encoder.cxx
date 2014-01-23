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

#include "config.h"
#include "encoder/EncoderList.hxx"
#include "encoder/EncoderPlugin.hxx"
#include "AudioFormat.hxx"
#include "config/ConfigData.hxx"
#include "stdbin.h"
#include "tag/Tag.hxx"
#include "tag/TagBuilder.hxx"
#include "util/Error.hxx"

#include <stddef.h>
#include <unistd.h>

static uint8_t zero[256];

static void
encoder_to_stdout(Encoder &encoder)
{
	size_t length;
	static char buffer[32768];

	while ((length = encoder_read(&encoder, buffer, sizeof(buffer))) > 0) {
		gcc_unused ssize_t ignored = write(1, buffer, length);
	}
}

int
main(gcc_unused int argc, gcc_unused char **argv)
{
	gcc_unused bool success;

	/* create the encoder */

	const auto plugin = encoder_plugin_get("vorbis");
	assert(plugin != NULL);

	config_param param;
	param.AddBlockParam("quality", "5.0", -1);

	const auto encoder = encoder_init(*plugin, param, IgnoreError());
	assert(encoder != NULL);

	/* open the encoder */

	AudioFormat audio_format(44100, SampleFormat::S16, 2);
	success = encoder_open(encoder, audio_format, IgnoreError());
	assert(success);

	encoder_to_stdout(*encoder);

	/* write a block of data */

	success = encoder_write(encoder, zero, sizeof(zero), IgnoreError());
	assert(success);

	encoder_to_stdout(*encoder);

	/* write a tag */

	success = encoder_pre_tag(encoder, IgnoreError());
	assert(success);

	encoder_to_stdout(*encoder);

	Tag tag;

	{
		TagBuilder tag_builder;
		tag_builder.AddItem(TAG_ARTIST, "Foo");
		tag_builder.AddItem(TAG_TITLE, "Bar");
		tag_builder.Commit(tag);
	}

	success = encoder_tag(encoder, &tag, IgnoreError());
	assert(success);

	encoder_to_stdout(*encoder);

	/* write another block of data */

	success = encoder_write(encoder, zero, sizeof(zero), IgnoreError());
	assert(success);

	/* finish */

	success = encoder_end(encoder, IgnoreError());
	assert(success);

	encoder_to_stdout(*encoder);

	encoder_close(encoder);
	encoder_finish(encoder);
}
