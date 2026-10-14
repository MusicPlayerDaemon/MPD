// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "OggEncoder.hxx"
#include "lib/xiph/OggPage.hxx"
#include "util/Serial.hxx"

OggEncoder::OggEncoder(bool _implements_tag) noexcept
	:Encoder(_implements_tag),
	 stream(GenerateSerial())
{
}

void
OggEncoder::Flush()
{
	flush = true;
}

std::span<const std::byte>
OggEncoder::Read([[maybe_unused]] std::span<std::byte> buffer) noexcept
{
	if (page_header.empty() && page_body.empty()) {
		ogg_page page;
		bool success = stream.PageOut(page);
		if (!success) {
			if (flush) {
				flush = false;
				success = stream.Flush(page);
			}

			if (!success)
				return {};
		}

		page_header = GetPageHeader(page);
		page_body = GetPageBody(page);
	}

	if (!page_header.empty())
		return std::exchange(page_header, {});

	assert(!page_body.empty());
	return std::exchange(page_body, {});
}
