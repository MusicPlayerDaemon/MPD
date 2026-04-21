// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include <string_view>

struct Tag;

class RemoteTagCacheHandler {
public:
	virtual void OnRemoteTag(std::string_view uri, const Tag &tag) noexcept = 0;
};
