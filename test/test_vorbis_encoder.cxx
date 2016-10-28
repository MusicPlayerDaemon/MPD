/*
 * Copyright 2003-2016 The Music Player Daemon Project
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

#include <memory>

#include <stddef.h>
#include <unistd.h>

static uint8_t zero[256];

int
main(gcc_unused int argc, gcc_unused char **argv)
try {
	gcc_unused bool success;

	/* create the encoder */

	const auto plugin = encoder_plugin_get("vorbis");
	assert(plugin != NULL);

	ConfigBlock block;
	block.AddBlockParam("quality", "5.0", -1);

	std::unique_ptr<PreparedEncoder> p_encoder(encoder_init(*plugin, block));
	assert(p_encoder != nullptr);

	/* open the encoder */

	AudioFormat audio_format(44100, SampleFormat::S16, 2);
	std::unique_ptr<Encoder> encoder(p_encoder->Open(audio_format,
							 IgnoreError()));
	assert(encoder != nullptr);

	StdioOutputStream os(stdout);

	EncoderToOutputStream(os, *encoder);

	/* write a block of data */

	success = encoder->Write(zero, sizeof(zero), IgnoreError());
	assert(success);

	EncoderToOutputStream(os, *encoder);

	/* write a tag */

	success = encoder->PreTag(IgnoreError());
	assert(success);

	EncoderToOutputStream(os, *encoder);

	Tag tag;

	{
		TagBuilder tag_builder;
		tag_builder.AddItem(TAG_ARTIST, "Foo");
		tag_builder.AddItem(TAG_TITLE, "Bar");
		tag_builder.Commit(tag);
	}

	success = encoder->SendTag(tag, IgnoreError());
	assert(success);

	EncoderToOutputStream(os, *encoder);

	/* write another block of data */

	success = encoder->Write(zero, sizeof(zero), IgnoreError());
	assert(success);

	/* finish */

	success = encoder->End(IgnoreError());
	assert(success);

	EncoderToOutputStream(os, *encoder);

	return EXIT_SUCCESS;
} catch (const std::exception &e) {
	LogError(e);
	return EXIT_FAILURE;
}
