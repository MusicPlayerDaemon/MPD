// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "OggSyncState.hxx"
#include "io/Reader.hxx"

bool
OggSyncState::Feed(size_t size)
{
	std::byte *buffer = reinterpret_cast<std::byte *>(ogg_sync_buffer(&oy, size));
	if (buffer == nullptr)
		return false;

	size_t nbytes = reader.Read({buffer, size});
	if (nbytes == 0)
		return false;

	ogg_sync_wrote(&oy, nbytes);
	return true;
}

bool
OggSyncState::ExpectPage(ogg_page &page)
{
	while (true) {
		int r = ogg_sync_pageout(&oy, &page);
		if (r != 0) {
			if (r > 0) {
				start_offset = offset;
				offset += page.header_len + page.body_len;
			}
			return r > 0;
		}

		if (!Feed(1024))
			return false;
	}
}

bool
OggSyncState::ExpectPageIn(ogg_stream_state &os)
{
	ogg_page page;
	if (!ExpectPage(page))
		return false;

	ogg_stream_pagein(&os, &page);
	return true;
}

bool
OggSyncState::ExpectPageSeek(ogg_page &page)
{
	size_t remaining_skipped = 65536;

	while (true) {
		int r = ogg_sync_pageseek(&oy, &page);
		if (r > 0) {
			start_offset = offset;
			offset += r;
			return true;
		}

		if (r < 0) {
			/* skipped -r bytes */
			size_t nbytes = -r;
			offset += nbytes;
			if (nbytes > remaining_skipped)
				/* still no ogg page - we lost our
				   patience, abort */
				return false;

			remaining_skipped -= nbytes;
			continue;
		}

		if (!Feed(1024))
			return false;
	}
}

bool
OggSyncState::ExpectPageSeekIn(ogg_stream_state &os)
{
	ogg_page page;
	if (!ExpectPageSeek(page))
		return false;

	ogg_stream_pagein(&os, &page);
	return true;
}
