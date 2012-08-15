/*
 * Copyright (C) 2003-2012 The Music Player Daemon Project
 * http://www.musicpd.org
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "config.h"
#include "ProxyDatabasePlugin.hxx"
#include "DatabasePlugin.hxx"
#include "DatabaseSelection.hxx"
#include "gcc.h"

extern "C" {
#include "db_error.h"
#include "conf.h"
#include "song.h"
#include "tag.h"
}

#include "directory.h"
#include "playlist_vector.h"

#undef MPD_DIRECTORY_H
#undef MPD_SONG_H
#include <mpd/client.h>

#include <cassert>
#include <string>
#include <list>

class ProxyDatabase : public Database {
	std::string host;
	unsigned port;

	struct mpd_connection *connection;
	struct directory *root;

public:
	static Database *Create(const struct config_param *param,
				GError **error_r);

	virtual bool Open(GError **error_r) override;
	virtual void Close() override;
	virtual struct song *GetSong(const char *uri_utf8,
				     GError **error_r) const override;
	virtual bool Visit(const DatabaseSelection &selection,
			   VisitDirectory visit_directory,
			   VisitSong visit_song,
			   VisitPlaylist visit_playlist,
			   GError **error_r) const override;

protected:
	bool Configure(const struct config_param *param, GError **error_r);
};

G_GNUC_CONST
static inline GQuark
libmpdclient_quark(void)
{
	return g_quark_from_static_string("libmpdclient");
}

static constexpr struct {
	enum tag_type d;
	enum mpd_tag_type s;
} tag_table[] = {
	{ TAG_ARTIST, MPD_TAG_ARTIST },
	{ TAG_ALBUM, MPD_TAG_ALBUM },
	{ TAG_ALBUM_ARTIST, MPD_TAG_ALBUM_ARTIST },
	{ TAG_TITLE, MPD_TAG_TITLE },
	{ TAG_TRACK, MPD_TAG_TRACK },
	{ TAG_NAME, MPD_TAG_NAME },
	{ TAG_GENRE, MPD_TAG_GENRE },
	{ TAG_DATE, MPD_TAG_DATE },
	{ TAG_COMPOSER, MPD_TAG_COMPOSER },
	{ TAG_PERFORMER, MPD_TAG_PERFORMER },
	{ TAG_COMMENT, MPD_TAG_COMMENT },
	{ TAG_DISC, MPD_TAG_DISC },
	{ TAG_MUSICBRAINZ_ARTISTID, MPD_TAG_MUSICBRAINZ_ARTISTID },
	{ TAG_MUSICBRAINZ_ALBUMID, MPD_TAG_MUSICBRAINZ_ALBUMID },
	{ TAG_MUSICBRAINZ_ALBUMARTISTID,
	  MPD_TAG_MUSICBRAINZ_ALBUMARTISTID },
	{ TAG_MUSICBRAINZ_TRACKID, MPD_TAG_MUSICBRAINZ_TRACKID },
	{ TAG_NUM_OF_ITEM_TYPES, MPD_TAG_COUNT }
};

static bool
CheckError(const struct mpd_connection *connection, GError **error_r)
{
	const auto error = mpd_connection_get_error(connection);
	if (error == MPD_ERROR_SUCCESS)
		return true;

	g_set_error_literal(error_r, libmpdclient_quark(), (int)error,
			    mpd_connection_get_error_message(connection));
	return false;
}

Database *
ProxyDatabase::Create(const struct config_param *param, GError **error_r)
{
	ProxyDatabase *db = new ProxyDatabase();
	if (!db->Configure(param, error_r)) {
		delete db;
		db = NULL;
	}

	return db;
}

bool
ProxyDatabase::Configure(const struct config_param *param, GError **)
{
	host = config_get_block_string(param, "host", "");
	port = config_get_block_unsigned(param, "port", 0);

	return true;
}

bool
ProxyDatabase::Open(GError **error_r)
{
	connection = mpd_connection_new(host.empty() ? NULL : host.c_str(),
					port, 0);
	if (connection == NULL) {
		g_set_error_literal(error_r, libmpdclient_quark(),
				    (int)MPD_ERROR_OOM, "Out of memory");
		return false;
	}

	if (!CheckError(connection, error_r)) {
		mpd_connection_free(connection);
		return false;
	}

	root = directory_new_root();

	return true;
}

void
ProxyDatabase::Close()
{
	assert(connection != nullptr);

	directory_free(root);
	mpd_connection_free(connection);
}

struct song *
ProxyDatabase::GetSong(const char *uri, GError **error_r) const
{
	// TODO: implement
	// TODO: auto-reconnect

	g_set_error(error_r, db_quark(), DB_NOT_FOUND,
		    "No such song: %s", uri);
	return nullptr;
}

static bool
Visit(struct mpd_connection *connection, directory &parent,
      bool recursive, VisitDirectory visit_directory, VisitSong visit_song,
      VisitPlaylist visit_playlist, GError **error_r);

static bool
Visit(struct mpd_connection *connection, directory &parent,
      bool recursive, const struct mpd_directory *directory,
      VisitDirectory visit_directory, VisitSong visit_song,
      VisitPlaylist visit_playlist, GError **error_r)
{
	if (!recursive && !visit_directory)
		return true;

	struct directory *d =
		directory_new(mpd_directory_get_path(directory), &parent);

	bool success = (!visit_directory || visit_directory(*d, error_r)) &&
		(!recursive ||
		 Visit(connection, *d, recursive,
		       visit_directory, visit_song, visit_playlist, error_r));
	directory_free(d);

	return success;
}

static void
Copy(struct tag *tag, enum tag_type d_tag,
     const struct mpd_song *song, enum mpd_tag_type s_tag)
{

	for (unsigned i = 0;; ++i) {
		const char *value = mpd_song_get_tag(song, s_tag, i);
		if (value == NULL)
			break;

		tag_add_item(tag, d_tag, value);
	}
}

static song *
Convert(struct directory &parent, const struct mpd_song *song)
{
	char *name = g_path_get_basename(mpd_song_get_uri(song));
	struct song *s = song_file_new(name, &parent);
	g_free(name);

	s->mtime = mpd_song_get_last_modified(song);
	s->start_ms = mpd_song_get_start(song) * 1000;
	s->end_ms = mpd_song_get_end(song) * 1000;

	struct tag *tag = tag_new();
	tag->time = mpd_song_get_duration(song);

	tag_begin_add(tag);
	for (auto i = tag_table; i->d != TAG_NUM_OF_ITEM_TYPES; ++i)
		Copy(tag, i->d, song, i->s);
	tag_end_add(tag);

	s->tag = tag;

	return s;
}

static bool
Visit(struct directory &parent, const struct mpd_song *song,
      VisitSong visit_song, GError **error_r)
{
	if (!visit_song)
		return true;

	struct song *s = Convert(parent, song);
	bool success = visit_song(*s, error_r);
	song_free(s);

	return success;
}

static bool
Visit(struct directory &parent, const struct mpd_playlist *playlist,
      VisitPlaylist visit_playlist, GError **error_r)
{
	if (!visit_playlist)
		return true;

	char *name = g_path_get_basename(mpd_playlist_get_path(playlist));
	struct playlist_metadata p;
	p.name = name;
	p.mtime = mpd_playlist_get_last_modified(playlist);

	bool success = visit_playlist(p, parent, error_r);
	g_free(name);

	return success;
}

class ProxyEntity {
	struct mpd_entity *entity;

public:
	explicit ProxyEntity(struct mpd_entity *_entity)
		:entity(_entity) {}

	ProxyEntity(const ProxyEntity &other) = delete;

	ProxyEntity(ProxyEntity &&other)
		:entity(other.entity) {
		other.entity = nullptr;
	}

	~ProxyEntity() {
		if (entity != nullptr)
			mpd_entity_free(entity);
	}

	ProxyEntity &operator=(const ProxyEntity &other) = delete;

	operator const struct mpd_entity *() const {
		return entity;
	}
};

static std::list<ProxyEntity>
ReceiveEntities(struct mpd_connection *connection)
{
	std::list<ProxyEntity> entities;
	struct mpd_entity *entity;
	while ((entity = mpd_recv_entity(connection)) != NULL)
		entities.push_back(ProxyEntity(entity));

	mpd_response_finish(connection);
	return entities;
}

static bool
Visit(struct mpd_connection *connection, struct directory &parent,
      bool recursive, VisitDirectory visit_directory, VisitSong visit_song,
      VisitPlaylist visit_playlist, GError **error_r)
{
	if (!mpd_send_list_meta(connection, directory_get_path(&parent)))
		return CheckError(connection, error_r);

	std::list<ProxyEntity> entities(ReceiveEntities(connection));
	if (!CheckError(connection, error_r))
		return false;

	for (const auto &entity : entities) {
		switch (mpd_entity_get_type(entity)) {
		case MPD_ENTITY_TYPE_UNKNOWN:
			break;

		case MPD_ENTITY_TYPE_DIRECTORY:
			if (!Visit(connection, parent, recursive,
				   mpd_entity_get_directory(entity),
				   visit_directory, visit_song, visit_playlist,
				   error_r))
				return false;
			break;

		case MPD_ENTITY_TYPE_SONG:
			if (!Visit(parent, mpd_entity_get_song(entity),
				   visit_song, error_r))
				return false;
			break;

		case MPD_ENTITY_TYPE_PLAYLIST:
			if (!Visit(parent, mpd_entity_get_playlist(entity),
				   visit_playlist, error_r))
				return false;
			break;
		}
	}

	return CheckError(connection, error_r);
}

bool
ProxyDatabase::Visit(const DatabaseSelection &selection,
		     VisitDirectory visit_directory,
		     VisitSong visit_song,
		     VisitPlaylist visit_playlist,
		     GError **error_r) const
{
	// TODO: match
	// TODO: auto-reconnect

	struct directory *parent = *selection.uri == 0
		? root
		: directory_new(selection.uri, root);
	bool success = ::Visit(connection, *parent, selection.recursive,
			       visit_directory, visit_song, visit_playlist,
			       error_r);
	if (parent != root)
		directory_free(parent);
	return success;
}

const DatabasePlugin proxy_db_plugin = {
	"proxy",
	ProxyDatabase::Create,
};
