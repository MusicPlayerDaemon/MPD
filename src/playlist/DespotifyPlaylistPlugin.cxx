/*
 * Copyright (C) 2011-2013 The Music Player Daemon Project
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
#include "DespotifyUtils.hxx"
#include "MemoryPlaylistProvider.hxx"
#include "tag.h"
#include "song.h"

extern "C" {
#include <despotify.h>
}

#include <glib.h>

#include <string.h>
#include <stdlib.h>

struct despotify_playlist {
	struct despotify_session *session;

	std::forward_list<SongPointer> songs;
};

static void
add_song(struct despotify_playlist *ctx, struct ds_track *track)
{
	const char *dsp_scheme = despotify_playlist_plugin.schemes[0];
	struct song *song;
	char uri[128];
	char *ds_uri;

	/* Create a spt://... URI for MPD */
	g_snprintf(uri, sizeof(uri), "%s://", dsp_scheme);
	ds_uri = uri + strlen(dsp_scheme) + 3;

	if (despotify_track_to_uri(track, ds_uri) != ds_uri) {
		/* Should never really fail, but let's be sure */
		g_debug("Can't add track %s\n", track->title);
		return;
	}

	song = song_remote_new(uri);
	song->tag = mpd_despotify_tag_from_track(track);

	ctx->songs.emplace_front(song);
}

static bool
parse_track(struct despotify_playlist *ctx,
		struct ds_link *link)
{
	struct ds_track *track;

	track = despotify_link_get_track(ctx->session, link);
	if (!track)
		return false;
	add_song(ctx, track);

	return true;
}

static bool
parse_playlist(struct despotify_playlist *ctx,
		struct ds_link *link)
{
	struct ds_playlist *playlist;
	struct ds_track *track;

	playlist = despotify_link_get_playlist(ctx->session, link);
	if (!playlist)
		return false;

	for (track = playlist->tracks; track; track = track->next)
		add_song(ctx, track);

	return true;
}

static bool
despotify_playlist_init(G_GNUC_UNUSED const struct config_param *param)
{
	return true;
}

static void
despotify_playlist_finish(void)
{
}


static struct playlist_provider *
despotify_playlist_open_uri(const char *url,
			    gcc_unused Mutex &mutex, gcc_unused Cond &cond)
{
	struct despotify_session *session;
	struct ds_link *link;
	bool parse_result;

	session = mpd_despotify_get_session();
	if (!session)
		return nullptr;

	/* Get link without spt:// */
	link = despotify_link_from_uri(url + strlen(despotify_playlist_plugin.schemes[0]) + 3);
	if (!link) {
		g_debug("Can't find %s\n", url);
		return nullptr;
	}

	struct despotify_playlist ctx;
	ctx.session = session;

	switch (link->type)
	{
	case LINK_TYPE_TRACK:
		parse_result = parse_track(&ctx, link);
		break;
	case LINK_TYPE_PLAYLIST:
		parse_result = parse_playlist(&ctx, link);
		break;
	default:
		parse_result = false;
		break;
	}
	despotify_free_link(link);
	if (!parse_result)
		return nullptr;

	ctx.songs.reverse();
	return new MemoryPlaylistProvider(std::move(ctx.songs));
}

static const char *const despotify_schemes[] = {
	"spt",
	nullptr
};

const struct playlist_plugin despotify_playlist_plugin = {
	"despotify",

	despotify_playlist_init,
	despotify_playlist_finish,

	despotify_playlist_open_uri,
	nullptr,
	nullptr,
	nullptr,

	despotify_schemes,
	nullptr,
	nullptr,
};
