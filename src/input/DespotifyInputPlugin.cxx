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
#include "DespotifyInputPlugin.hxx"
#include "DespotifyUtils.hxx"
#include "InputInternal.hxx"
#include "InputStream.hxx"
#include "InputPlugin.hxx"
#include "tag.h"

extern "C" {
#include <despotify.h>
}

#include <glib.h>

#include <unistd.h>
#include <string.h>
#include <errno.h>

#include <stdio.h>

struct DespotifyInputStream {
	struct input_stream base;

	struct despotify_session *session;
	struct ds_track *track;
	struct tag *tag;
	struct ds_pcm_data pcm;
	size_t len_available;
	bool eof;

	DespotifyInputStream(const char *uri,
			     Mutex &mutex, Cond &cond,
			     despotify_session *_session,
			     ds_track *_track)
		:base(input_plugin_despotify, uri, mutex, cond),
		 session(_session), track(_track),
		 tag(mpd_despotify_tag_from_track(track)),
		 len_available(0), eof(false) {

		memset(&pcm, 0, sizeof(pcm));

		/* Despotify outputs pcm data */
		base.mime = g_strdup("audio/x-mpd-cdda-pcm");
		base.ready = true;
	}

	~DespotifyInputStream() {
		if (tag != NULL)
			tag_free(tag);

		despotify_free_track(track);
	}
};

static void
refill_buffer(DespotifyInputStream *ctx)
{
	/* Wait until there is data */
	while (1) {
		int rc = despotify_get_pcm(ctx->session, &ctx->pcm);

		if (rc == 0 && ctx->pcm.len) {
			ctx->len_available = ctx->pcm.len;
			break;
		}
		if (ctx->eof == true)
			break;

		if (rc < 0) {
			g_debug("despotify_get_pcm error\n");
			ctx->eof = true;
			break;
		}

		/* Wait a while until next iteration */
		usleep(50 * 1000);
	}
}

static void callback(G_GNUC_UNUSED struct despotify_session* ds,
		int sig, G_GNUC_UNUSED void* data, void* callback_data)
{
	DespotifyInputStream *ctx = (DespotifyInputStream *)callback_data;

	switch (sig) {
	case DESPOTIFY_NEW_TRACK:
		break;

	case DESPOTIFY_TIME_TELL:
		break;

	case DESPOTIFY_TRACK_PLAY_ERROR:
		g_debug("Track play error\n");
		ctx->eof = true;
		ctx->len_available = 0;
		break;

	case DESPOTIFY_END_OF_PLAYLIST:
		ctx->eof = true;
		g_debug("End of playlist: %d\n", ctx->eof);
		break;
	}
}


static struct input_stream *
input_despotify_open(const char *url,
		     Mutex &mutex, Cond &cond,
		     G_GNUC_UNUSED GError **error_r)
{
	struct despotify_session *session;
	struct ds_link *ds_link;
	struct ds_track *track;

	if (!g_str_has_prefix(url, "spt://"))
		return NULL;

	session = mpd_despotify_get_session();
	if (!session)
		return NULL;

	ds_link = despotify_link_from_uri(url + 6);
	if (!ds_link) {
		g_debug("Can't find %s\n", url);
		return NULL;
	}
	if (ds_link->type != LINK_TYPE_TRACK) {
		despotify_free_link(ds_link);
		return NULL;
	}

	track = despotify_link_get_track(session, ds_link);
	despotify_free_link(ds_link);
	if (!track)
		return NULL;

	DespotifyInputStream *ctx =
		new DespotifyInputStream(url, mutex, cond,
					 session, track);

	if (!mpd_despotify_register_callback(callback, ctx)) {
		delete ctx;
		return NULL;
	}

	if (despotify_play(ctx->session, ctx->track, false) == false) {
		mpd_despotify_unregister_callback(callback);
		delete ctx;
		return NULL;
	}

	return &ctx->base;
}

static size_t
input_despotify_read(struct input_stream *is, void *ptr, size_t size,
	       G_GNUC_UNUSED GError **error_r)
{
	DespotifyInputStream *ctx = (DespotifyInputStream *)is;
	size_t to_cpy = size;

	if (ctx->len_available == 0)
		refill_buffer(ctx);

	if (ctx->len_available < size)
		to_cpy = ctx->len_available;
	memcpy(ptr, ctx->pcm.buf, to_cpy);
	ctx->len_available -= to_cpy;

	is->offset += to_cpy;

	return to_cpy;
}

static void
input_despotify_close(struct input_stream *is)
{
	DespotifyInputStream *ctx = (DespotifyInputStream *)is;

	mpd_despotify_unregister_callback(callback);
	delete ctx;
}

static bool
input_despotify_eof(struct input_stream *is)
{
	DespotifyInputStream *ctx = (DespotifyInputStream *)is;

	return ctx->eof;
}

static bool
input_despotify_seek(G_GNUC_UNUSED struct input_stream *is,
	       G_GNUC_UNUSED goffset offset, G_GNUC_UNUSED int whence,
	       G_GNUC_UNUSED GError **error_r)
{
	return false;
}

static struct tag *
input_despotify_tag(struct input_stream *is)
{
	DespotifyInputStream *ctx = (DespotifyInputStream *)is;
	struct tag *tag = ctx->tag;

	ctx->tag = NULL;

	return tag;
}

const struct input_plugin input_plugin_despotify = {
	"spt",
	nullptr,
	nullptr,
	input_despotify_open,
	input_despotify_close,
	nullptr,
	nullptr,
	.tag = input_despotify_tag,
	nullptr,
	input_despotify_read,
	input_despotify_eof,
	input_despotify_seek,
};
