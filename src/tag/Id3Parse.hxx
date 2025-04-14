// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include "Id3Unique.hxx"

#include <id3tag.h>

#include <cstddef>
#include <span>

/**
 * Wrapper for id3_tag_parse() which accepts a std::span and returns a
 * #UniqueId3Tag.
 */
inline UniqueId3Tag
id3_tag_parse(std::span<const std::byte> src) noexcept
{
	return UniqueId3Tag{id3_tag_parse(reinterpret_cast<const id3_byte_t *>(src.data()), src.size())};
}
