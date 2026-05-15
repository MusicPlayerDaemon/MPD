// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "SongSticker.hxx"
#include "Sticker.hxx"
#include "Database.hxx"
#include "song/LightSong.hxx"
#include "db/Interface.hxx"
#include "co/Generator.hxx"
#include "util/AllocatedString.hxx"
#include "util/StringCompare.hxx"

#include <string.h>
#include <stdlib.h>

using std::string_view_literals::operator""sv;

std::string
sticker_song_get_value(StickerDatabase &db,
		       const LightSong &song, const char *name)
{
	const auto uri = song.GetURI();
	return db.LoadValue("song", uri.c_str(), name);
}

void
sticker_song_set_value(StickerDatabase &db,
		       const LightSong &song,
		       const char *name, const char *value)
{
	const auto uri = song.GetURI();
	db.StoreValue("song", uri.c_str(), name, value);
}

void
sticker_song_inc_value(StickerDatabase &db,
		       const LightSong &song,
		       const char *name, const char *value)
{
	const auto uri = song.GetURI();
	db.IncValue("song", uri.c_str(), name, value);
}

void
sticker_song_dec_value(StickerDatabase &db,
		       const LightSong &song,
		       const char *name, const char *value)
{
	const auto uri = song.GetURI();
	db.DecValue("song", uri.c_str(), name, value);
}

bool
sticker_song_delete(StickerDatabase &db, const char *uri)
{
	return db.Delete("song", uri);
}

bool
sticker_song_delete(StickerDatabase &db, const LightSong &song)
{
	return sticker_song_delete(db, song.GetURI().c_str());
}

bool
sticker_song_delete_value(StickerDatabase &db,
			  const LightSong &song, const char *name)
{
	const auto uri = song.GetURI();
	return db.DeleteValue("song", uri.c_str(), name);
}

Sticker
sticker_song_get(StickerDatabase &db, const LightSong &song)
{
	const auto uri = song.GetURI();
	return db.Load("song", uri.c_str());
}

Co::Generator<FindSongStickerRecord>
sticker_song_find(StickerDatabase &sticker_database, const Database &db,
		  const char *base_uri, const char *name,
		  StickerOperator op, const char *value,
		  const char *sort, bool descending, RangeArg window)
{
	std::string_view base_uri_sv{base_uri};

	AllocatedString allocated;
	if (!base_uri_sv.empty()) {
		/* append slash to base_uri */
		allocated = AllocatedString{base_uri_sv, "/"sv};
		base_uri = allocated.c_str();
		base_uri_sv = base_uri;
	} else {
		/* searching in root directory - no trailing slash */
	}

	for (const auto &i : sticker_database.Find("song", base_uri, name, op, value,
						   sort, descending, window)) {
		if (!StringStartsWith(i.uri, base_uri_sv))
			/* should not happen, ignore silently */
			continue;

		try {
			const LightSong *song = db.GetSong(i.uri);
			co_yield FindSongStickerRecord{*song, i.value};
			db.ReturnSong(song);
		} catch (...) {
		}
	}
}
