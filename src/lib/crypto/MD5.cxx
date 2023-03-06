// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "MD5.hxx"
#include "util/HexFormat.hxx"
#include "config.h"

#ifdef HAVE_LIBAVUTIL
extern "C" {
#include <libavutil/md5.h>
}
#else
#include "lib/gcrypt/MD5.hxx"
#include "lib/gcrypt/Init.hxx"
#endif

void
GlobalInitMD5() noexcept
{
#ifdef HAVE_LIBAVUTIL
	/* no initialization necessary */
#else
	Gcrypt::Init();
#endif
}

std::array<std::byte, 16>
MD5(std::span<const std::byte> input) noexcept
{
#ifdef HAVE_LIBAVUTIL
	std::array<std::byte, 16> result;
	av_md5_sum((uint8_t *)result.data(),
		   (const uint8_t *)input.data(), input.size());
	return result;
#else
	return Gcrypt::MD5(input);
#endif
}

std::array<char, 32>
MD5Hex(std::span<const std::byte> input) noexcept
{
	const auto raw = MD5(input);
	return HexFormat<raw.size()>(raw);
}
