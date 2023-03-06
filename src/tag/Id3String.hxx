// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include <id3tag.h>

#include <stdlib.h>

/**
 * A UTF-8 string allocated by libid3tag.
 */
class Id3String {
	id3_utf8_t *p = nullptr;

	Id3String(id3_utf8_t *_p) noexcept:p(_p) {}

public:
	Id3String() noexcept = default;

	~Id3String() noexcept {
		free(p);
	}

	Id3String(const Id3String &) = delete;
	Id3String &operator=(const Id3String &) = delete;

	static Id3String FromUCS4(const id3_ucs4_t *ucs4) noexcept {
		return id3_ucs4_utf8duplicate(ucs4);
	}

	operator bool() const noexcept {
		return p != nullptr;
	}

	const char *c_str() const noexcept {
		return (const char *)p;
	}
};
