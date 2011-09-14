/*
 * Copyright (C) 2011 The Music Player Daemon Project
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
#include "input/despotify_input_plugin.h"
#include "input_internal.h"
#include "input_plugin.h"
#include "tag.h"
#include "despotify_utils.h"

#include <glib.h>

#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <despotify.h>

#include <stdio.h>

struct input_despotify {
	struct input_stream base;

	struct despotify_session *session;
	struct ds_track *track;
	struct tag *tag;
	struct ds_pcm_data pcm;
	size_t len_available;
	bool eof;
};


static void
refill_buffer(struct input_despotify *ctx)
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
	struct input_despotify *ctx = (struct input_despotify *)callback_data;

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
		     GMutex *mutex, GCond *cond,
		     G_GNUC_UNUSED GError **error_r)
{
	struct input_despotify *ctx;
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

	ctx = g_new(struct input_despotify, 1);
	memset(ctx, 0, sizeof(*ctx));

	track = despotify_link_get_track(session, ds_link);
	despotify_free_link(ds_link);
	if (!track) {
		g_free(ctx);
		return NULL;
	}

	input_stream_init(&ctx->base, &input_plugin_despotify, url,
			  mutex, cond);
	ctx->session = session;
	ctx->track = track;
	ctx->tag = mpd_despotify_tag_from_track(track);
	ctx->eof = false;
	/* Despotify outputs pcm data */
	ctx->base.mime = g_strdup("audio/x-mpd-cdda-pcm");
	ctx->base.ready = true;

	if (!mpd_despotify_register_callback(callback, ctx)) {
		despotify_free_link(ds_link);

		return NULL;
	}

	if (despotify_play(ctx->session, ctx->track, false) == false) {
		despotify_free_track(ctx->track);
		g_free(ctx);
		return NULL;
	}

	return &ctx->base;
}

static size_t
input_despotify_read(struct input_stream *is, void *ptr, size_t size,
	       G_GNUC_UNUSED GError **error_r)
{
	struct input_despotify *ctx = (struct input_despotify *)is;
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
	struct input_despotify *ctx = (struct input_despotify *)is;

	if (ctx->tag != NULL)
		tag_free(ctx->tag);

	mpd_despotify_unregister_callback(callback);
	despotify_free_track(ctx->track);
	input_stream_deinit(&ctx->base);
	g_free(ctx);
}

static bool
input_despotify_eof(struct input_stream *is)
{
	struct input_despotify *ctx = (struct input_despotify *)is;

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
	struct input_despotify *ctx = (struct input_despotify *)is;
	struct tag *tag = ctx->tag;

	ctx->tag = NULL;

	return tag;
}

const struct input_plugin input_plugin_despotify = {
	.name = "spt",
	.open = input_despotify_open,
	.close = input_despotify_close,
	.read = input_despotify_read,
	.eof = input_despotify_eof,
	.seek = input_despotify_seek,
	.tag = input_despotify_tag,
};
