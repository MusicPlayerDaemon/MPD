// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

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
	template<typename U>
	explicit FailingInputStream(U &&_uri,
				    const std::exception_ptr _error,
				    Mutex &_mutex) noexcept
		:InputStream(std::forward<U>(_uri), _mutex), error(_error) {
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

	size_t Read(std::unique_lock<Mutex> &, std::span<std::byte>) override {
		std::rethrow_exception(error);
	}
};

#endif
