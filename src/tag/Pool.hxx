// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_TAG_POOL_HXX
#define MPD_TAG_POOL_HXX

#include "thread/Mutex.hxx"

#include <cstdint>
#include <string_view>

enum TagType : uint8_t;

extern Mutex tag_pool_lock;

struct TagItem;

[[nodiscard]]
TagItem *
tag_pool_get_item(TagType type, std::string_view value) noexcept;

[[nodiscard]]
TagItem *
tag_pool_dup_item(TagItem *item) noexcept;

void
tag_pool_put_item(TagItem *item) noexcept;

#endif
