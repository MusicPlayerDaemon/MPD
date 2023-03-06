// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_OGG_PAGE_HXX
#define MPD_OGG_PAGE_HXX

#include <ogg/ogg.h>

#include <cassert>
#include <cstdint>

#include <string.h>

static size_t
ReadPage(const ogg_page &page, void *_buffer, size_t size) noexcept
{
	assert(page.header_len > 0 || page.body_len > 0);

	size_t header_len = (size_t)page.header_len;
	size_t body_len = (size_t)page.body_len;
	assert(header_len <= size);

	if (header_len + body_len > size)
		/* TODO: better overflow handling */
		body_len = size - header_len;

	uint8_t *buffer = (uint8_t *)_buffer;
	memcpy(buffer, page.header, header_len);
	memcpy(buffer + header_len, page.body, body_len);

	return header_len + body_len;
}

#endif
