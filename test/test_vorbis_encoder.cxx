/*
 * Copyright (C) 2003-2015 The Music Player Daemon Project
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
#include "encoder/EncoderInterface.hxx"
#include "encoder/ToOutputStream.hxx"
#include "AudioFormat.hxx"
#include "config/Block.hxx"
#include "fs/io/StdioOutputStream.hxx"
#include "tag/Tag.hxx"
#include "tag/TagBuilder.hxx"
#include "util/Error.hxx"
#include "Log.hxx"

#include <stddef.h>
#include <unistd.h>

static uint8_t zero[256];

int
main(gcc_unused int argc, gcc_unused char **argv)
{
	gcc_unused bool success;

	/* create the encoder */

	const auto plugin = encoder_plugin_get("vorbis");
	assert(plugin != NULL);

	ConfigBlock block;
	block.AddBlockParam("quality", "5.0", -1);

	const auto encoder = encoder_init(*plugin, block, IgnoreError());
	assert(encoder != NULL);

	/* open the encoder */

	AudioFormat audio_format(44100, SampleFormat::S16, 2);
	success = encoder->Open(audio_format, IgnoreError());
	assert(success);

	StdioOutputStream os(stdout);

	Error error;
	if (!EncoderToOutputStream(os, *encoder, error)) {
		LogError(error);
		return EXIT_FAILURE;
	}

	/* write a block of data */

	success = encoder_write(encoder, zero, sizeof(zero), IgnoreError());
	assert(success);

	if (!EncoderToOutputStream(os, *encoder, error)) {
		LogError(error);
		return EXIT_FAILURE;
	}

	/* write a tag */

	success = encoder_pre_tag(encoder, IgnoreError());
	assert(success);

	if (!EncoderToOutputStream(os, *encoder, error)) {
		LogError(error);
		return EXIT_FAILURE;
	}

	Tag tag;

	{
		TagBuilder tag_builder;
		tag_builder.AddItem(TAG_ARTIST, "Foo");
		tag_builder.AddItem(TAG_TITLE, "Bar");
		tag_builder.Commit(tag);
	}

	success = encoder_tag(encoder, tag, IgnoreError());
	assert(success);

	if (!EncoderToOutputStream(os, *encoder, error)) {
		LogError(error);
		return EXIT_FAILURE;
	}

	/* write another block of data */

	success = encoder_write(encoder, zero, sizeof(zero), IgnoreError());
	assert(success);

	/* finish */

	success = encoder_end(encoder, IgnoreError());
	assert(success);

	if (!EncoderToOutputStream(os, *encoder, error)) {
		LogError(error);
		return EXIT_FAILURE;
	}

	encoder->Close();
	encoder->Dispose();
}
