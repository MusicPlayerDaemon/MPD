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

#ifndef MPD_DECODER_READER_HXX
#define MPD_DECODER_READER_HXX

#include "check.h"
#include "fs/io/Reader.hxx"
#include "Compiler.h"

struct Decoder;
class InputStream;

/**
 * A wrapper for decoder_read() which implements the #Reader
 * interface.
 */
class DecoderReader final : public Reader {
	Decoder &decoder;
	InputStream &is;

public:
	DecoderReader(Decoder &_decoder, InputStream &_is)
		:decoder(_decoder), is(_is) {}

	/* virtual methods from class Reader */
	size_t Read(void *data, size_t size) override;
};

#endif
