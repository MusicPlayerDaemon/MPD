// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "SongSticker.hxx"
#include "Sticker.hxx"
#include "Database.hxx"
#include "song/LightSong.hxx"
#include "db/Interface.hxx"
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

namespace {
struct sticker_song_find_data {
	const Database *db;
	std::string_view base_uri;

	BoundMethod<void(const LightSong &song, const char *value)> func;
};
} // namespace

static void
sticker_song_find_cb(const char *uri, const char *value, void *user_data)
{
	auto *data =
		(struct sticker_song_find_data *)user_data;

	if (!StringStartsWith(uri, data->base_uri))
		/* should not happen, ignore silently */
		return;

	const Database *db = data->db;
	try {
		const LightSong *song = db->GetSong(uri);
		data->func(*song, value);
		db->ReturnSong(song);
	} catch (...) {
	}
}

void
sticker_song_find(StickerDatabase &sticker_database, const Database &db,
		  const char *base_uri, const char *name,
		  StickerOperator op, const char *value,
		  const char *sort, bool descending, RangeArg window,
		  BoundMethod<void(const LightSong &song, const char *value)> func)
{
	struct sticker_song_find_data data{
		.db = &db,
		.func = func,
	};

	data.base_uri = base_uri;

	AllocatedString allocated;
	if (!data.base_uri.empty()) {
		/* append slash to base_uri */
		allocated = AllocatedString{data.base_uri, "/"sv};
		base_uri = allocated.c_str();
		data.base_uri = base_uri;
	} else {
		/* searching in root directory - no trailing slash */
	}

	sticker_database.Find("song", base_uri, name, op, value,
				  sort, descending, window,
			      sticker_song_find_cb, &data);
}
