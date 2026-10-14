// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include <ogg/ogg.h>

#include <span>

static inline std::span<const std::byte>
GetPageHeader(const ogg_page &page) noexcept
{
	return {
		reinterpret_cast<const std::byte *>(page.header),
		static_cast<std::size_t>(page.header_len),
	};
}

static inline std::span<const std::byte>
GetPageBody(const ogg_page &page) noexcept
{
	return {
		reinterpret_cast<const std::byte *>(page.body),
		static_cast<std::size_t>(page.body_len),
	};
}
