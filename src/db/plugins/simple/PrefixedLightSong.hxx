// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_DB_SIMPLE_PREFIXED_LIGHT_SONG_HXX
#define MPD_DB_SIMPLE_PREFIXED_LIGHT_SONG_HXX

#include "song/LightSong.hxx"
#include "fs/Traits.hxx"

#include <string>

class PrefixedLightSong : public LightSong {
	std::string buffer;

public:
	template<typename B>
	PrefixedLightSong(const LightSong &song, B &&base)
		:LightSong(song),
		 buffer(PathTraitsUTF8::Build(std::forward<B>(base),
					      GetURI()))
	{
		uri = buffer.c_str();
		directory = nullptr;
	}
};

#endif
