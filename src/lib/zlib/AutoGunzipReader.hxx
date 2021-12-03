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

#ifndef MPD_AUTO_GUNZIP_READER_HXX
#define MPD_AUTO_GUNZIP_READER_HXX

#include "io/PeekReader.hxx"

#include <memory>

class GunzipReader;

/**
 * A filter that detects gzip compression and optionally inserts a
 * #GunzipReader.
 */
class AutoGunzipReader final : public Reader {
	Reader *next = nullptr;
	PeekReader peek;
	std::unique_ptr<GunzipReader> gunzip;

public:
	explicit AutoGunzipReader(Reader &_next) noexcept;
	~AutoGunzipReader() noexcept;

	/* virtual methods from class Reader */
	size_t Read(void *data, size_t size) override;

private:
	void Detect();
};

#endif
