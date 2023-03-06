// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "AutoGunzipReader.hxx"
#include "GunzipReader.hxx"

AutoGunzipReader::AutoGunzipReader(Reader &_next) noexcept
	:peek(_next) {}

AutoGunzipReader::~AutoGunzipReader() noexcept = default;

[[gnu::pure]]
static bool
IsGzip(const uint8_t data[4]) noexcept
{
	return data[0] == 0x1f && data[1] == 0x8b && data[2] == 0x08 &&
		(data[3] & 0xe0) == 0;
}

inline void
AutoGunzipReader::Detect()
{
	const auto *data = (const uint8_t *)peek.Peek(4);
	if (data == nullptr) {
		next = &peek;
		return;
	}

	if (IsGzip(data))
		next = (gunzip = std::make_unique<GunzipReader>(peek)).get();
	else
		next = &peek;
}

size_t
AutoGunzipReader::Read(void *data, size_t size)
{
	if (next == nullptr)
		Detect();

	assert(next != nullptr);
	return next->Read(data, size);
}
