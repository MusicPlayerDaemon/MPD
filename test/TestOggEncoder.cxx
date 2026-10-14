// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "encoder/EncoderList.hxx"
#include "encoder/EncoderPlugin.hxx"
#include "encoder/EncoderInterface.hxx"
#include "pcm/AudioFormat.hxx"
#include "config/Block.hxx"
#include "tag/Tag.hxx"
#include "tag/Builder.hxx"

#include <ogg/ogg.h>

#include <gtest/gtest.h>

#include <cstring>
#include <memory>
#include <string>
#include <vector>

/**
 * Read everything from the encoder, using a buffer which is smaller
 * than some Ogg pages.
 */
static void
Drain(Encoder &encoder, std::vector<std::byte> &output)
{
	std::byte buffer[1000];

	while (true) {
		const auto r = encoder.Read(buffer);
		if (r.empty())
			break;

		output.insert(output.end(), r.begin(), r.end());
	}
}

TEST(OggEncoder, LargePage)
{
	const auto plugin = encoder_plugin_get("opus");
	if (plugin == nullptr)
		GTEST_SKIP() << "No Opus encoder";

	const ConfigBlock block;
	std::unique_ptr<PreparedEncoder> p_encoder(encoder_init(*plugin, block));

	AudioFormat audio_format(48000, SampleFormat::S16, 2);
	std::unique_ptr<Encoder> encoder(p_encoder->Open(audio_format));

	std::vector<std::byte> output;
	Drain(*encoder, output);

	static constexpr std::byte zero[3840]{};
	encoder->Write(std::span{zero});
	Drain(*encoder, output);

	/* a tag which produces a comment header page larger than the
	   buffer passed to Read() (and larger than the 32 kB buffers
	   used by the output plugins) */
	const std::string title(40000, 'x');

	Tag tag;
	{
		TagBuilder tag_builder;
		tag_builder.AddItem(TAG_TITLE, title);
		tag_builder.Commit(tag);
	}

	encoder->PreTag();
	Drain(*encoder, output);
	encoder->SendTag(tag);
	Drain(*encoder, output);

	encoder->Write(std::span{zero});
	encoder->End();
	Drain(*encoder, output);

	/* parse the output: all pages must be intact (no bytes
	   skipped because of a bad CRC or truncated page), and the
	   comment header must contain the whole title */

	ogg_sync_state sync;
	ogg_sync_init(&sync);

	char *p = ogg_sync_buffer(&sync, output.size());
	memcpy(p, output.data(), output.size());
	ogg_sync_wrote(&sync, output.size());

	bool found_title = false;
	unsigned n_pages = 0;
	ogg_stream_state stream;
	bool stream_initialized = false;

	ogg_page page;
	while (true) {
		const int result = ogg_sync_pageseek(&sync, &page);
		ASSERT_GE(result, 0) << "garbage in the Ogg stream";
		if (result == 0)
			break;

		++n_pages;

		if (ogg_page_bos(&page)) {
			if (stream_initialized)
				ogg_stream_clear(&stream);
			ogg_stream_init(&stream, ogg_page_serialno(&page));
			stream_initialized = true;
		}

		ASSERT_TRUE(stream_initialized);
		ASSERT_EQ(ogg_stream_pagein(&stream, &page), 0);

		ogg_packet packet;
		while (ogg_stream_packetout(&stream, &packet) == 1) {
			if (packet.bytes > 8 &&
			    memcmp(packet.packet, "OpusTags", 8) == 0 &&
			    std::string_view{(const char *)packet.packet,
					     (std::size_t)packet.bytes}.find(title) != std::string_view::npos)
				found_title = true;
		}
	}

	/* all bytes must have been consumed */
	EXPECT_EQ(sync.returned, sync.fill);
	EXPECT_GT(n_pages, 0U);
	EXPECT_TRUE(found_title);

	if (stream_initialized)
		ogg_stream_clear(&stream);
	ogg_sync_clear(&sync);
}
