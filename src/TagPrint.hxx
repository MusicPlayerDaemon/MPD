// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include <cstdint>
#include <string_view>

enum TagType : uint8_t;

struct Tag;
class Response;

void
tag_print_types(Response &response) noexcept;

void
tag_print_types_available(Response &response) noexcept;

void
tag_print(Response &response, TagType type, std::string_view value) noexcept;

void
tag_print(Response &response, TagType type, const char *value) noexcept;

/**
 * A version of tag_print() that sanitizes the value using
 * FixTagString() before sending it.
 */
void
tag_print_sanitized(Response &response, TagType type, std::string_view value) noexcept;

void
tag_print_values(Response &response, const Tag &tag) noexcept;

void
tag_print(Response &response, const Tag &tag) noexcept;
