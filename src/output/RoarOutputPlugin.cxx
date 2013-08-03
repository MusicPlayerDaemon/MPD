/*
 * Copyright (C) 2003-2013 The Music Player Daemon Project
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
#include "RoarOutputPlugin.hxx"
#include "OutputAPI.hxx"
#include "MixerList.hxx"
#include "thread/Mutex.hxx"

#include <glib.h>

/* libroar/services.h declares roar_service_stream::new - work around
   this C++ problem */
#define new _new
#include <roaraudio.h>
#undef new

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "roaraudio"

struct RoarOutput {
	struct audio_output base;

	roar_vs_t * vss;
	int err;
	char *host;
	char *name;
	int role;
	struct roar_connection con;
	struct roar_audio_info info;
	Mutex mutex;
	volatile bool alive;

	RoarOutput()
		:err(ROAR_ERROR_NONE),
		 host(nullptr), name(nullptr) {}

	~RoarOutput() {
		g_free(host);
		g_free(name);
	}
};

static inline GQuark
roar_output_quark(void)
{
	return g_quark_from_static_string("roar_output");
}

static int
roar_output_get_volume_locked(RoarOutput *roar)
{
	if (roar->vss == nullptr || !roar->alive)
		return -1;

	float l, r;
	int error;
	if (roar_vs_volume_get(roar->vss, &l, &r, &error) < 0)
		return -1;

	return (l + r) * 50;
}

int
roar_output_get_volume(RoarOutput *roar)
{
	const ScopeLock protect(roar->mutex);
	return roar_output_get_volume_locked(roar);
}

static bool
roar_output_set_volume_locked(RoarOutput *roar, unsigned volume)
{
	assert(volume <= 100);

	if (roar->vss == nullptr || !roar->alive)
		return false;

	int error;
	float level = volume / 100.0;

	roar_vs_volume_mono(roar->vss, level, &error);
	return true;
}

bool
roar_output_set_volume(RoarOutput *roar, unsigned volume)
{
	const ScopeLock protect(roar->mutex);
	return roar_output_set_volume_locked(roar, volume);
}

static void
roar_configure(RoarOutput *self, const struct config_param *param)
{
	self->host = config_dup_block_string(param, "server", nullptr);
	self->name = config_dup_block_string(param, "name", "MPD");

	const char *role = config_get_block_string(param, "role", "music");
	self->role = role != nullptr
		? roar_str2role(role)
		: ROAR_ROLE_MUSIC;
}

static struct audio_output *
roar_init(const struct config_param *param, GError **error_r)
{
	RoarOutput *self = new RoarOutput();

	if (!ao_base_init(&self->base, &roar_output_plugin, param, error_r)) {
		delete self;
		return nullptr;
	}

	roar_configure(self, param);
	return &self->base;
}

static void
roar_finish(struct audio_output *ao)
{
	RoarOutput *self = (RoarOutput *)ao;

	ao_base_finish(&self->base);
	delete self;
}

static void
roar_use_audio_format(struct roar_audio_info *info,
		      AudioFormat &audio_format)
{
	info->rate = audio_format.sample_rate;
	info->channels = audio_format.channels;
	info->codec = ROAR_CODEC_PCM_S;

	switch (audio_format.format) {
	case SampleFormat::UNDEFINED:
	case SampleFormat::FLOAT:
	case SampleFormat::DSD:
		info->bits = 16;
		audio_format.format = SampleFormat::S16;
		break;

	case SampleFormat::S8:
		info->bits = 8;
		break;

	case SampleFormat::S16:
		info->bits = 16;
		break;

	case SampleFormat::S24_P32:
		info->bits = 32;
		audio_format.format = SampleFormat::S32;
		break;

	case SampleFormat::S32:
		info->bits = 32;
		break;
	}
}

static bool
roar_open(struct audio_output *ao, AudioFormat &audio_format, GError **error)
{
	RoarOutput *self = (RoarOutput *)ao;
	const ScopeLock protect(self->mutex);

	if (roar_simple_connect(&(self->con), self->host, self->name) < 0)
	{
		g_set_error(error, roar_output_quark(), 0,
				"Failed to connect to Roar server");
		return false;
	}

	self->vss = roar_vs_new_from_con(&(self->con), &(self->err));

	if (self->vss == nullptr || self->err != ROAR_ERROR_NONE)
	{
		g_set_error(error, roar_output_quark(), 0,
				"Failed to connect to server");
		return false;
	}

	roar_use_audio_format(&self->info, audio_format);

	if (roar_vs_stream(self->vss, &(self->info), ROAR_DIR_PLAY,
				&(self->err)) < 0)
	{
		g_set_error(error, roar_output_quark(), 0, "Failed to start stream");
		return false;
	}
	roar_vs_role(self->vss, self->role, &(self->err));
	self->alive = true;

	return true;
}

static void
roar_close(struct audio_output *ao)
{
	RoarOutput *self = (RoarOutput *)ao;
	const ScopeLock protect(self->mutex);

	self->alive = false;

	if (self->vss != nullptr)
		roar_vs_close(self->vss, ROAR_VS_TRUE, &(self->err));
	self->vss = nullptr;
	roar_disconnect(&(self->con));
}

static void
roar_cancel_locked(RoarOutput *self)
{
	if (self->vss == nullptr)
		return;

	roar_vs_t *vss = self->vss;
	self->vss = nullptr;
	roar_vs_close(vss, ROAR_VS_TRUE, &(self->err));
	self->alive = false;

	vss = roar_vs_new_from_con(&(self->con), &(self->err));
	if (vss == nullptr)
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
	RoarOutput *self = (RoarOutput *)ao;

	const ScopeLock protect(self->mutex);
	roar_cancel_locked(self);
}

static size_t
roar_play(struct audio_output *ao, const void *chunk, size_t size, GError **error)
{
	RoarOutput *self = (RoarOutput *)ao;
	ssize_t rc;

	if (self->vss == nullptr)
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
			return nullptr;
	}
}

static void
roar_send_tag(struct audio_output *ao, const Tag *meta)
{
	RoarOutput *self = (RoarOutput *)ao;

	if (self->vss == nullptr)
		return;

	const ScopeLock protect(self->mutex);

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
		if (key != nullptr)
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
}

const struct audio_output_plugin roar_output_plugin = {
	"roar",
	nullptr,
	roar_init,
	roar_finish,
	nullptr,
	nullptr,
	roar_open,
	roar_close,
	nullptr,
	roar_send_tag,
	roar_play,
	nullptr,
	roar_cancel,
	nullptr,
	&roar_mixer_plugin,
};
