/*
 * Copyright (C) 2003-2015 The Music Player Daemon Project
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
#include "DespotifyPlaylistPlugin.hxx"
#include "lib/despotify/DespotifyUtils.hxx"
#include "../PlaylistPlugin.hxx"
#include "../MemorySongEnumerator.hxx"
#include "tag/Tag.hxx"
#include "DetachedSong.hxx"
#include "Log.hxx"

extern "C" {
#include <despotify.h>
}

#include <string.h>
#include <stdlib.h>
#include <string.h>

static void
add_song(std::forward_list<DetachedSong> &songs, ds_track &track)
{
	const char *dsp_scheme = despotify_playlist_plugin.schemes[0];
	char uri[128];
	char *ds_uri;

	/* Create a spt://... URI for MPD */
	snprintf(uri, sizeof(uri), "%s://", dsp_scheme);
	ds_uri = uri + strlen(dsp_scheme) + 3;

	if (despotify_track_to_uri(&track, ds_uri) != ds_uri) {
		/* Should never really fail, but let's be sure */
		FormatDebug(despotify_domain,
			    "Can't add track %s", track.title);
		return;
	}

	songs.emplace_front(uri, mpd_despotify_tag_from_track(track));
}

static bool
parse_track(struct despotify_session *session,
	    std::forward_list<DetachedSong> &songs,
	    struct ds_link *link)
{
	struct ds_track *track = despotify_link_get_track(session, link);
	if (track == nullptr)
		return false;

	add_song(songs, *track);
	return true;
}

static bool
parse_playlist(struct despotify_session *session,
	       std::forward_list<DetachedSong> &songs,
	       struct ds_link *link)
{
	ds_playlist *playlist = despotify_link_get_playlist(session, link);
	if (playlist == nullptr)
		return false;

	for (ds_track *track = playlist->tracks; track != nullptr;
	     track = track->next)
		add_song(songs, *track);

	return true;
}

static SongEnumerator *
despotify_playlist_open_uri(const char *url,
			    gcc_unused Mutex &mutex, gcc_unused Cond &cond)
{
	despotify_session *session = mpd_despotify_get_session();
	if (session == nullptr)
		return nullptr;

	/* Get link without spt:// */
	ds_link *link =
		despotify_link_from_uri(url + strlen(despotify_playlist_plugin.schemes[0]) + 3);
	if (link == nullptr) {
		FormatDebug(despotify_domain, "Can't find %s\n", url);
		return nullptr;
	}

	std::forward_list<DetachedSong> songs;

	bool parse_result;
	switch (link->type) {
	case LINK_TYPE_TRACK:
		parse_result = parse_track(session, songs, link);
		break;
	case LINK_TYPE_PLAYLIST:
		parse_result = parse_playlist(session, songs, link);
		break;
	default:
		parse_result = false;
		break;
	}

	despotify_free_link(link);
	if (!parse_result)
		return nullptr;

	songs.reverse();
	return new MemorySongEnumerator(std::move(songs));
}

static const char *const despotify_schemes[] = {
	"spt",
	nullptr
};

const struct playlist_plugin despotify_playlist_plugin = {
	"despotify",

	nullptr,
	nullptr,
	despotify_playlist_open_uri,
	nullptr,

	despotify_schemes,
	nullptr,
	nullptr,
};
