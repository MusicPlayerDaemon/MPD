// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include "Page.hxx"

#include <cstddef>
#include <cstdint>
#include <string>

enum TagType : uint8_t;
struct Tag;

std::string
icy_server_metadata_header(const char *name,
			   const char *genre, const char *url,
			   const char *content_type, std::size_t metaint) noexcept;

PagePtr
icy_server_metadata_page(const Tag &tag, const TagType *types) noexcept;
