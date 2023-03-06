// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

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
