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

#ifndef MPD_ICY_INPUT_STREAM_HXX
#define MPD_ICY_INPUT_STREAM_HXX

#include "ProxyInputStream.hxx"
#include "IcyMetaDataParser.hxx"
#include "Compiler.h"

struct Tag;

/**
 * An #InputStream filter that parses Icy metadata.
 */
class IcyInputStream final : public ProxyInputStream {
	IcyMetaDataParser parser;

	/**
	 * The #Tag object ready to be requested via ReadTag().
	 */
	Tag *input_tag;

	/**
	 * The #Tag object ready to be requested via ReadTag().
	 */
	Tag *icy_tag;

	offset_type override_offset;

public:
	IcyInputStream(InputStream *_input);
	virtual ~IcyInputStream();

	IcyInputStream(const IcyInputStream &) = delete;
	IcyInputStream &operator=(const IcyInputStream &) = delete;

	void Enable(size_t _data_size) {
		parser.Start(_data_size);
	}

	bool IsEnabled() const {
		return parser.IsDefined();
	}

	/* virtual methods from InputStream */
	void Update() override;
	Tag *ReadTag() override;
	size_t Read(void *ptr, size_t size, Error &error) override;
};

#endif
