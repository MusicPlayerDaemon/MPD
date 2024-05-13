// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

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
		    std::span<std::byte> dest) override;

private:
	/* virtual methods from class BufferingInputStream */
	void OnBufferAvailable() noexcept override {
		InvokeOnAvailable();
	}
};
