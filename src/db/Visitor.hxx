// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_DATABASE_VISITOR_HXX
#define MPD_DATABASE_VISITOR_HXX

#include <functional>

struct LightDirectory;
struct LightSong;
struct PlaylistInfo;
struct Tag;

typedef std::function<void(const LightDirectory &)> VisitDirectory;
typedef std::function<void(const LightSong &)> VisitSong;
typedef std::function<void(const PlaylistInfo &,
			   const LightDirectory &)> VisitPlaylist;

typedef std::function<void(const Tag &)> VisitTag;

#endif
