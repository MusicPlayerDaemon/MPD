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

#ifndef MPD_BUFFERED_INPUT_STREAM_BUFFER_HXX
#define MPD_BUFFERED_INPUT_STREAM_BUFFER_HXX

#include "InputStream.hxx"
#include "BufferingInputStream.hxx"

#include <cassert>

/**
 * A "huge" buffer which remembers the (partial) contents of an
 * #InputStream.  This works only if the #InputStream is a "file", not
 * a "stream"; see IsEligible() for details.
 */
class BufferedInputStream final : public InputStream, BufferingInputStream {
	// TODO: make configurable
	static constexpr offset_type MAX_SIZE = 128 * 1024 * 1024;

public:
	BufferedInputStream(InputStreamPtr _input);

	/**
	 * Check whether the given #InputStream can be used as input
	 * for this class.
	 */
	static bool IsEligible(const InputStream &input) noexcept {
		assert(input.IsReady());

		return input.IsSeekable() && input.KnownSize() &&
			input.GetSize() > 0 &&
			input.GetSize() <= MAX_SIZE;
	}

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
	/* virtual methods from class BufferingInputStream */
	void OnBufferAvailable() noexcept override {
		InvokeOnAvailable();
	}
};

#endif
