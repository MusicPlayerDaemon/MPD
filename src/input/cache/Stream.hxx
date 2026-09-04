// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include "Lease.hxx"
#include "input/InputStream.hxx"

/**
 * An #InputStream implementation which reads data from an
 * #InputCacheItem.
 */
class CacheInputStream final : public InputStream {
	class Lease final : public InputCacheLease {
		CacheInputStream &stream;

	public:
		explicit Lease(CacheInputStream &_stream) noexcept
			:stream(_stream) {}

		void Set(InputCacheLease &&src) noexcept {
			InputCacheLease::operator=(std::move(src));
		}

	private:
		void OnInputCacheAvailable(std::unique_lock<Mutex> &lock) noexcept override {
			stream.OnInputCacheAvailable(lock);
		}
	};

	Lease lease;

public:
	CacheInputStream(InputCacheLease _lease, Mutex &_mutex) noexcept;
	~CacheInputStream() noexcept override;

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
	void OnInputCacheAvailable(std::unique_lock<Mutex> &lock) noexcept;
};
