// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "SongSticker.hxx"
#include "Sticker.hxx"
#include "Database.hxx"
#include "song/LightSong.hxx"
#include "db/Interface.hxx"
#include "util/AllocatedString.hxx"

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
	const char *base_uri;
	size_t base_uri_length;

	void (*func)(const LightSong &song, const char *value,
		     void *user_data);
	void *user_data;
};
} // namespace

static void
sticker_song_find_cb(const char *uri, const char *value, void *user_data)
{
	auto *data =
		(struct sticker_song_find_data *)user_data;

	if (memcmp(uri, data->base_uri, data->base_uri_length) != 0)
		/* should not happen, ignore silently */
		return;

	const Database *db = data->db;
	try {
		const LightSong *song = db->GetSong(uri);
		data->func(*song, value, data->user_data);
		db->ReturnSong(song);
	} catch (...) {
	}
}

void
sticker_song_find(StickerDatabase &sticker_database, const Database &db,
		  const char *base_uri, const char *name,
		  StickerOperator op, const char *value,
		  void (*func)(const LightSong &song, const char *value,
			       void *user_data),
		  void *user_data)
{
	struct sticker_song_find_data data;
	data.db = &db;
	data.func = func;
	data.user_data = user_data;

	AllocatedString allocated;
	data.base_uri = base_uri;
	if (*data.base_uri != 0) {
		/* append slash to base_uri */
		allocated = AllocatedString{std::string_view{data.base_uri}, "/"sv};
		data.base_uri = allocated.c_str();
	} else {
		/* searching in root directory - no trailing slash */
	}

	data.base_uri_length = strlen(data.base_uri);

	sticker_database.Find("song", data.base_uri, name, op, value,
			      sticker_song_find_cb, &data);
}
