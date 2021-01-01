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

#ifndef MPD_CACHE_INPUT_STREAM_HXX
#define MPD_CACHE_INPUT_STREAM_HXX

#include "Lease.hxx"
#include "input/InputStream.hxx"

/**
 * An #InputStream implementation which reads data from an
 * #InputCacheItem.
 */
class CacheInputStream final : public InputStream, InputCacheLease {
public:
	CacheInputStream(InputCacheLease _lease, Mutex &_mutex) noexcept;

	/* virtual methods from class InputStream */
	void Check() override;
	/* we don't need to implement Update() because all attributes
	   have been copied already in our constructor */
	//void Update() noexcept;
	void Seek(std::unique_lock<Mutex> &lock, offset_type offset) override;
	bool IsEOF() const noexcept override;
	/* we don't support tags */
	// std::unique_ptr<Tag> ReadTag() override;
	bool IsAvailable() const noexcept override;
	size_t Read(std::unique_lock<Mutex> &lock,
		    void *ptr, size_t size) override;

private:
	/* virtual methods from class InputCacheLease */
	void OnInputCacheAvailable() noexcept override;
};

#endif
