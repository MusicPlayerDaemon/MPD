// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_OPUS_READER_HXX
#define MPD_OPUS_READER_HXX

#include <algorithm>
#include <cstdint>
#include <string_view>

#include <string.h>

class OpusReader {
	const uint8_t *p, *const end;

public:
	constexpr OpusReader(const void *_p, size_t size) noexcept
		:p((const uint8_t *)_p), end(p + size) {}

	constexpr bool Skip(size_t length) noexcept {
		p += length;
		return p <= end;
	}

	constexpr const void *Read(size_t length) noexcept {
		const uint8_t *result = p;
		return Skip(length)
			? result
			: nullptr;
	}

	bool Expect(const void *value, size_t length) noexcept {
		const void *data = Read(length);
		return data != nullptr && memcmp(value, data, length) == 0;
	}

	constexpr bool ReadByte(uint8_t &value_r) noexcept {
		if (p >= end)
			return false;

		value_r = *p++;
		return true;
	}

	constexpr bool ReadShort(uint16_t &value_r) noexcept {
		const uint8_t *value = (const uint8_t *)Read(sizeof(value_r));
		if (value == nullptr)
			return false;

		value_r = value[0] | (value[1] << 8);
		return true;
	}

	constexpr bool ReadWord(uint32_t &value_r) noexcept {
		const uint8_t *value = (const uint8_t *)Read(sizeof(value_r));
		if (value == nullptr)
			return false;

		value_r = value[0] | (value[1] << 8)
			| (value[2] << 16) | (value[3] << 24);
		return true;
	}

	constexpr bool SkipString() noexcept {
		uint32_t length;
		return ReadWord(length) && Skip(length);
	}

	constexpr std::string_view ReadString() noexcept {
		uint32_t length;
		if (!ReadWord(length))
			return {};

		const char *src = (const char *)Read(length);
		if (src == nullptr)
			return {};

		return {src, length};
	}
};

#endif
