// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_TAG_PRINT_HXX
#define MPD_TAG_PRINT_HXX

#include <cstdint>
#include <string_view>

enum TagType : uint8_t;

struct Tag;
class Response;

void
tag_print_types(Response &response) noexcept;

void
tag_print(Response &response, TagType type, std::string_view value) noexcept;

void
tag_print(Response &response, TagType type, const char *value) noexcept;

void
tag_print_values(Response &response, const Tag &tag) noexcept;

void
tag_print(Response &response, const Tag &tag) noexcept;

#endif
