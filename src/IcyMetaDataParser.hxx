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

#ifndef MPD_ICY_META_DATA_PARSER_HXX
#define MPD_ICY_META_DATA_PARSER_HXX

#include <stddef.h>

struct Tag;

class IcyMetaDataParser {
	size_t data_size, data_rest;

	size_t meta_size, meta_position;
	char *meta_data;

	Tag *tag;

public:
	IcyMetaDataParser():data_size(0) {}
	~IcyMetaDataParser() {
		Reset();
	}

	/**
	 * Initialize an enabled icy_metadata object with the specified
	 * data_size (from the icy-metaint HTTP response header).
	 */
	void Start(size_t _data_size) {
		data_size = data_rest = _data_size;
		meta_size = 0;
		tag = nullptr;
	}

	/**
	 * Resets the icy_metadata.  Call this after rewinding the stream.
	 */
	void Reset();

	/**
	 * Checks whether the icy_metadata object is enabled.
	 */
	bool IsDefined() const {
		return data_size > 0;
	}

	/**
	 * Evaluates data.  Returns the number of bytes of normal data which
	 * can be read by the caller, but not more than "length".  If the
	 * return value is smaller than "length", the caller should invoke
	 * icy_meta().
	 */
	size_t Data(size_t length);

	/**
	 * Reads metadata from the stream.  Returns the number of bytes
	 * consumed.  If the return value is smaller than "length", the caller
	 * should invoke icy_data().
	 */
	size_t Meta(const void *data, size_t length);

	/**
	 * Parse data and eliminate metadata.
	 *
	 * @return the number of data bytes remaining in the buffer
	 */
	size_t ParseInPlace(void *data, size_t length);

	Tag *ReadTag() {
		Tag *result = tag;
		tag = nullptr;
		return result;
	}
};

#endif
