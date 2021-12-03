/*
 * Copyright 2003-2021 The Music Player Daemon Project
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

#include "encoder/EncoderList.hxx"
#include "encoder/EncoderPlugin.hxx"
#include "encoder/EncoderInterface.hxx"
#include "encoder/ToOutputStream.hxx"
#include "pcm/AudioFormat.hxx"
#include "config/Block.hxx"
#include "io/StdioOutputStream.hxx"
#include "tag/Tag.hxx"
#include "tag/Builder.hxx"
#include "util/PrintException.hxx"

#include <cassert>
#include <memory>

#include <stddef.h>

static uint8_t zero[256];

int
main([[maybe_unused]] int argc, [[maybe_unused]] char **argv)
try {
	/* create the encoder */

	const auto plugin = encoder_plugin_get("vorbis");
	assert(plugin != nullptr);

	ConfigBlock block;
	block.AddBlockParam("quality", "5.0", -1);

	std::unique_ptr<PreparedEncoder> p_encoder(encoder_init(*plugin, block));
	assert(p_encoder != nullptr);

	/* open the encoder */

	AudioFormat audio_format(44100, SampleFormat::S16, 2);
	std::unique_ptr<Encoder> encoder(p_encoder->Open(audio_format));
	assert(encoder != nullptr);

	StdioOutputStream os(stdout);

	EncoderToOutputStream(os, *encoder);

	/* write a block of data */

	encoder->Write(zero, sizeof(zero));

	EncoderToOutputStream(os, *encoder);

	/* write a tag */

	encoder->PreTag();

	EncoderToOutputStream(os, *encoder);

	Tag tag;

	{
		TagBuilder tag_builder;
		tag_builder.AddItem(TAG_ARTIST, "Foo");
		tag_builder.AddItem(TAG_TITLE, "Bar");
		tag_builder.Commit(tag);
	}

	encoder->SendTag(tag);

	EncoderToOutputStream(os, *encoder);

	/* write another block of data */

	encoder->Write(zero, sizeof(zero));

	/* finish */

	encoder->End();
	EncoderToOutputStream(os, *encoder);

	return EXIT_SUCCESS;
} catch (...) {
	PrintException(std::current_exception());
	return EXIT_FAILURE;
}
