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
