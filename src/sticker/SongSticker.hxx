// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_SONG_STICKER_HXX
#define MPD_SONG_STICKER_HXX

#include "Match.hxx"
#include "protocol/RangeArg.hxx"

#include <string>

struct LightSong;
struct Sticker;
class Database;
class StickerDatabase;

/**
 * Returns one value from a song's sticker record.
 *
 * Throws #SqliteError on error.
 */
std::string
sticker_song_get_value(StickerDatabase &db,
		       const LightSong &song, const char *name);

/**
 * Sets a sticker value in the specified song.  Overwrites existing
 * values.
 *
 * Throws #SqliteError on error.
 */
void
sticker_song_set_value(StickerDatabase &db,
		       const LightSong &song,
		       const char *name, const char *value);

/**
 * Deletes a sticker from the database.  All values are deleted.
 *
 * Throws #SqliteError on error.
 */
bool
sticker_song_delete(StickerDatabase &db, const char *uri);

bool
sticker_song_delete(StickerDatabase &db, const LightSong &song);

/**
 * Deletes a sticker value.  Does nothing if the sticker did not
 * exist.
 *
 * Throws #SqliteError on error.
 */
bool
sticker_song_delete_value(StickerDatabase &db,
			  const LightSong &song, const char *name);

/**
 * Loads the sticker for the specified song.
 *
 * Throws #SqliteError on error.
 *
 * @param song the song object
 * @return a sticker object
 */
Sticker
sticker_song_get(StickerDatabase &db, const LightSong &song);

/**
 * Finds stickers with the specified name below the specified
 * directory.
 *
 * Caller must lock the #db_mutex.
 *
 * Throws #SqliteError on error.
 *
 * @param base_uri the base directory to search in
 * @param name the name of the sticker
 */
void
sticker_song_find(StickerDatabase &sticker_database, const Database &db,
		  const char *base_uri, const char *name,
		  StickerOperator op, const char *value,
		  const char *sort, bool descending, RangeArg window,
		  void (*func)(const LightSong &song, const char *value,
			       void *user_data),
		  void *user_data);

#endif
