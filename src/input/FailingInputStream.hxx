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

#ifndef MPD_FAILING_INPUT_STREAM_HXX
#define MPD_FAILING_INPUT_STREAM_HXX

#include "InputStream.hxx"

#include <exception>

/**
 * An #InputStream which always fails.  This is useful for
 * ProxyInputStream::SetInput() if the implementation fails to
 * initialize the inner #InputStream instance.
 */
class FailingInputStream final : public InputStream {
	const std::exception_ptr error;

public:
	explicit FailingInputStream(const char *_uri,
				    const std::exception_ptr _error,
				    Mutex &_mutex) noexcept
		:InputStream(_uri, _mutex), error(_error) {
		SetReady();
	}

	/* virtual methods from InputStream */
	void Check() override {
		std::rethrow_exception(error);
	}

	void Seek(std::unique_lock<Mutex> &, offset_type) override {
		std::rethrow_exception(error);
	}

	bool IsEOF() const noexcept override {
		return false;
	}

	size_t Read(std::unique_lock<Mutex> &, void *, size_t) override {
		std::rethrow_exception(error);
	}
};

#endif
