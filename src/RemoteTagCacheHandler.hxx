// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_REMOTE_TAG_CACHE_HANDLER_HXX
#define MPD_REMOTE_TAG_CACHE_HANDLER_HXX

struct Tag;

class RemoteTagCacheHandler {
public:
	virtual void OnRemoteTag(const char *uri, const Tag &tag) noexcept = 0;
};

#endif
