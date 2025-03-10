// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "InputStream.hxx"

class MemoryInputStream final : public InputStream {
	std::span<const std::byte> src;

public:
	MemoryInputStream(const char *_uri, Mutex &_mutex,
			  std::span<const std::byte> _src) noexcept
		:InputStream(_uri, _mutex), src(_src)
	{
		size = src.size();
		seekable = true;
		SetReady();
	}

	/* virtual methods from InputStream */

	[[nodiscard]] bool IsEOF() const noexcept override {
		return GetOffset() >= GetSize();
	}

	size_t Read(std::unique_lock<Mutex> &lock,
		    std::span<std::byte> dest) override;
	void Seek(std::unique_lock<Mutex> &lock,
		  offset_type offset) override;
};
