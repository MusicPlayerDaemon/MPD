// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include <cstdint>
#include <string_view>

enum TagType : uint8_t;
struct Partition;
class Response;
class SongFilter;

void
PrintSongCount(Response &r, const Partition &partition, std::string_view name,
	       const SongFilter *filter,
	       TagType group);
