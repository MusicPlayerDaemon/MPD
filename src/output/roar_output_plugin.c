/*
 * Copyright (C) 2003-2010 The Music Player Daemon Project
 * Copyright (C) 2010-2011 Philipp 'ph3-der-loewe' Schafft
 * Copyright (C) 2010-2011 Hans-Kristian 'maister' Arntzen
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
#include "roar_output_plugin.h"
#include "output_api.h"
#include "mixer_list.h"
#include "roar_output_plugin.h"

#include <glib.h>
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <roaraudio.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "roaraudio"

typedef struct roar
{
	struct audio_output base;

	roar_vs_t * vss;
	int err;
	char *host;
	char *name;
	int role;
	struct roar_connection con;
	struct roar_audio_info info;
	GMutex *lock;
	volatile bool alive;
} roar_t;

static inline GQuark
roar_output_quark(void)
{
	return g_quark_from_static_string("roar_output");
}

static int
roar_output_get_volume_locked(struct roar *roar)
{
	if (roar->vss == NULL || !roar->alive)
		return -1;

	float l, r;
	int error;
	if (roar_vs_volume_get(roar->vss, &l, &r, &error) < 0)
		return -1;

	return (l + r) * 50;
}

int
roar_output_get_volume(struct roar *roar)
{
	g_mutex_lock(roar->lock);
	int volume = roar_output_get_volume_locked(roar);
	g_mutex_unlock(roar->lock);
	return volume;
}

static bool
roar_output_set_volume_locked(struct roar *roar, unsigned volume)
{
	assert(volume <= 100);

	if (roar->vss == NULL || !roar->alive)
		return false;

	int error;
	float level = volume / 100.0;

	roar_vs_volume_mono(roar->vss, level, &error);
	return true;
}

bool
roar_output_set_volume(struct roar *roar, unsigned volume)
{
	g_mutex_lock(roar->lock);
	bool success = roar_output_set_volume_locked(roar, volume);
	g_mutex_unlock(roar->lock);
	return success;
}

static void
roar_configure(struct roar * self, const struct config_param *param)
{
	self->host = config_dup_block_string(param, "server", NULL);
	self->name = config_dup_block_string(param, "name", "MPD");

	const char *role = config_get_block_string(param, "role", "music");
	self->role = role != NULL
		? roar_str2role(role)
		: ROAR_ROLE_MUSIC;
}

static struct audio_output *
roar_init(const struct config_param *param, GError **error_r)
{
	struct roar *self = g_new0(struct roar, 1);

	if (!ao_base_init(&self->base, &roar_output_plugin, param, error_r)) {
		g_free(self);
		return NULL;
	}

	self->lock = g_mutex_new();
	self->err = ROAR_ERROR_NONE;
	roar_configure(self, param);
	return &self->base;
}

static void
roar_finish(struct audio_output *ao)
{
	struct roar *self = (struct roar *)ao;

	g_free(self->host);
	g_free(self->name);
	g_mutex_free(self->lock);

	ao_base_finish(&self->base);
	g_free(self);
}

static void
roar_use_audio_format(struct roar_audio_info *info,
		      struct audio_format *audio_format)
{
	info->rate = audio_format->sample_rate;
	info->channels = audio_format->channels;
	info->codec = ROAR_CODEC_PCM_S;

	switch (audio_format->format) {
	case SAMPLE_FORMAT_UNDEFINED:
		info->bits = 16;
		audio_format->format = SAMPLE_FORMAT_S16;
		break;

	case SAMPLE_FORMAT_S8:
		info->bits = 8;
		break;

	case SAMPLE_FORMAT_S16:
		info->bits = 16;
		break;

	case SAMPLE_FORMAT_S24_P32:
		info->bits = 32;
		audio_format->format = SAMPLE_FORMAT_S32;
		break;

	case SAMPLE_FORMAT_S32:
		info->bits = 32;
		break;
	}
}

static bool
roar_open(struct audio_output *ao, struct audio_format *audio_format, GError **error)
{
	struct roar *self = (struct roar *)ao;
	g_mutex_lock(self->lock);

	if (roar_simple_connect(&(self->con), self->host, self->name) < 0)
	{
		g_set_error(error, roar_output_quark(), 0,
				"Failed to connect to Roar server");
		g_mutex_unlock(self->lock);
		return false;
	}

	self->vss = roar_vs_new_from_con(&(self->con), &(self->err));

	if (self->vss == NULL || self->err != ROAR_ERROR_NONE)
	{
		g_set_error(error, roar_output_quark(), 0,
				"Failed to connect to server");
		g_mutex_unlock(self->lock);
		return false;
	}

	roar_use_audio_format(&self->info, audio_format);

	if (roar_vs_stream(self->vss, &(self->info), ROAR_DIR_PLAY,
				&(self->err)) < 0)
	{
		g_set_error(error, roar_output_quark(), 0, "Failed to start stream");
		g_mutex_unlock(self->lock);
		return false;
	}
	roar_vs_role(self->vss, self->role, &(self->err));
	self->alive = true;

	g_mutex_unlock(self->lock);
	return true;
}

static void
roar_close(struct audio_output *ao)
{
	struct roar *self = (struct roar *)ao;
	g_mutex_lock(self->lock);
	self->alive = false;

	if (self->vss != NULL)
		roar_vs_close(self->vss, ROAR_VS_TRUE, &(self->err));
	self->vss = NULL;
	roar_disconnect(&(self->con));
	g_mutex_unlock(self->lock);
}

static void
roar_cancel_locked(struct roar *self)
{
	if (self->vss == NULL)
		return;

	roar_vs_t *vss = self->vss;
	self->vss = NULL;
	roar_vs_close(vss, ROAR_VS_TRUE, &(self->err));
	self->alive = false;

	vss = roar_vs_new_from_con(&(self->con), &(self->err));
	if (vss == NULL)
		return;

	if (roar_vs_stream(vss, &(self->info), ROAR_DIR_PLAY,
			   &(self->err)) < 0) {
		roar_vs_close(vss, ROAR_VS_TRUE, &(self->err));
		g_warning("Failed to start stream");
		return;
	}

	roar_vs_role(vss, self->role, &(self->err));
	self->vss = vss;
	self->alive = true;
}

static void
roar_cancel(struct audio_output *ao)
{
	struct roar *self = (struct roar *)ao;

	g_mutex_lock(self->lock);
	roar_cancel_locked(self);
	g_mutex_unlock(self->lock);
}

static size_t
roar_play(struct audio_output *ao, const void *chunk, size_t size, GError **error)
{
	struct roar *self = (struct roar *)ao;
	ssize_t rc;

	if (self->vss == NULL)
	{
		g_set_error(error, roar_output_quark(), 0, "Connection is invalid");
		return 0;
	}

	rc = roar_vs_write(self->vss, chunk, size, &(self->err));
	if ( rc <= 0 )
	{
		g_set_error(error, roar_output_quark(), 0, "Failed to play data");
		return 0;
	}

	return rc;
}

static const char*
roar_tag_convert(enum tag_type type, bool *is_uuid)
{
	*is_uuid = false;
	switch (type)
	{
		case TAG_ARTIST:
		case TAG_ALBUM_ARTIST:
			return "AUTHOR";
		case TAG_ALBUM:
			return "ALBUM";
		case TAG_TITLE:
			return "TITLE";
		case TAG_TRACK:
			return "TRACK";
		case TAG_NAME:
			return "NAME";
		case TAG_GENRE:
			return "GENRE";
		case TAG_DATE:
			return "DATE";
		case TAG_PERFORMER:
			return "PERFORMER";
		case TAG_COMMENT:
			return "COMMENT";
		case TAG_DISC:
			return "DISCID";
		case TAG_COMPOSER:
#ifdef ROAR_META_TYPE_COMPOSER
			return "COMPOSER";
#else
			return "AUTHOR";
#endif
		case TAG_MUSICBRAINZ_ARTISTID:
		case TAG_MUSICBRAINZ_ALBUMID:
		case TAG_MUSICBRAINZ_ALBUMARTISTID:
		case TAG_MUSICBRAINZ_TRACKID:
			*is_uuid = true;
			return "HASH";

		default:
			return NULL;
	}
}

static void
roar_send_tag(struct audio_output *ao, const struct tag *meta)
{
	struct roar *self = (struct roar *)ao;

	if (self->vss == NULL)
		return;

	g_mutex_lock(self->lock);
	size_t cnt = 1;
	struct roar_keyval vals[32];
	memset(vals, 0, sizeof(vals));
	char uuid_buf[32][64];

	char timebuf[16];
	snprintf(timebuf, sizeof(timebuf), "%02d:%02d:%02d",
			meta->time / 3600, (meta->time % 3600) / 60, meta->time % 60);

	vals[0].key = g_strdup("LENGTH");
	vals[0].value = timebuf;

	for (unsigned i = 0; i < meta->num_items && cnt < 32; i++)
	{
		bool is_uuid = false;
		const char *key = roar_tag_convert(meta->items[i]->type, &is_uuid);
		if (key != NULL)
		{
			if (is_uuid)
			{
				snprintf(uuid_buf[cnt], sizeof(uuid_buf[0]), "{UUID}%s",
						meta->items[i]->value);
				vals[cnt].key = g_strdup(key);
				vals[cnt].value = uuid_buf[cnt];
			}
			else
			{
				vals[cnt].key = g_strdup(key);
				vals[cnt].value = meta->items[i]->value;
			}
			cnt++;
		}
	}

	roar_vs_meta(self->vss, vals, cnt, &(self->err));

	for (unsigned i = 0; i < 32; i++)
		g_free(vals[i].key);

	g_mutex_unlock(self->lock);
}

const struct audio_output_plugin roar_output_plugin = {
	.name = "roar",
	.init = roar_init,
	.finish = roar_finish,
	.open = roar_open,
	.play = roar_play,
	.cancel = roar_cancel,
	.close = roar_close,
	.send_tag = roar_send_tag,

	.mixer_plugin = &roar_mixer_plugin
};
