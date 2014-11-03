/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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
#include "../OutputAPI.hxx"
#include "mixer/MixerList.hxx"
#include "thread/Mutex.hxx"
#include "util/Error.hxx"
#include "util/Domain.hxx"
#include "Log.hxx"

#include <string>

/* libroar/services.h declares roar_service_stream::new - work around
   this C++ problem */
#define new _new
#include <roaraudio.h>
#undef new

class RoarOutput {
	AudioOutput base;

	std::string host, name;

	roar_vs_t * vss;
	int err;
	int role;
	struct roar_connection con;
	struct roar_audio_info info;
	mutable Mutex mutex;
	bool alive;

public:
	RoarOutput()
		:base(roar_output_plugin),
		 err(ROAR_ERROR_NONE) {}

	operator AudioOutput *() {
		return &base;
	}

	bool Initialize(const config_param &param, Error &error) {
		return base.Configure(param, error);
	}

	void Configure(const config_param &param);

	bool Open(AudioFormat &audio_format, Error &error);
	void Close();

	void SendTag(const Tag &tag);
	size_t Play(const void *chunk, size_t size, Error &error);
	void Cancel();

	int GetVolume() const;
	bool SetVolume(unsigned volume);
};

static constexpr Domain roar_output_domain("roar_output");

inline int
RoarOutput::GetVolume() const
{
	const ScopeLock protect(mutex);

	if (vss == nullptr || !alive)
		return -1;

	float l, r;
	int error;
	if (roar_vs_volume_get(vss, &l, &r, &error) < 0)
		return -1;

	return (l + r) * 50;
}

int
roar_output_get_volume(RoarOutput &roar)
{
	return roar.GetVolume();
}

bool
RoarOutput::SetVolume(unsigned volume)
{
	assert(volume <= 100);

	const ScopeLock protect(mutex);
	if (vss == nullptr || !alive)
		return false;

	int error;
	float level = volume / 100.0;

	roar_vs_volume_mono(vss, level, &error);
	return true;
}

bool
roar_output_set_volume(RoarOutput &roar, unsigned volume)
{
	return roar.SetVolume(volume);
}

inline void
RoarOutput::Configure(const config_param &param)
{
	host = param.GetBlockValue("server", "");
	name = param.GetBlockValue("name", "MPD");

	const char *_role = param.GetBlockValue("role", "music");
	role = _role != nullptr
		? roar_str2role(_role)
		: ROAR_ROLE_MUSIC;
}

static AudioOutput *
roar_init(const config_param &param, Error &error)
{
	RoarOutput *self = new RoarOutput();

	if (!self->Initialize(param, error)) {
		delete self;
		return nullptr;
	}

	self->Configure(param);
	return *self;
}

static void
roar_finish(AudioOutput *ao)
{
	RoarOutput *self = (RoarOutput *)ao;

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

inline bool
RoarOutput::Open(AudioFormat &audio_format, Error &error)
{
	const ScopeLock protect(mutex);

	if (roar_simple_connect(&con,
				host.empty() ? nullptr : host.c_str(),
				name.c_str()) < 0) {
		error.Set(roar_output_domain,
			  "Failed to connect to Roar server");
		return false;
	}

	vss = roar_vs_new_from_con(&con, &err);

	if (vss == nullptr || err != ROAR_ERROR_NONE) {
		error.Set(roar_output_domain, "Failed to connect to server");
		return false;
	}

	roar_use_audio_format(&info, audio_format);

	if (roar_vs_stream(vss, &info, ROAR_DIR_PLAY, &err) < 0) {
		error.Set(roar_output_domain, "Failed to start stream");
		return false;
	}

	roar_vs_role(vss, role, &err);
	alive = true;
	return true;
}

static bool
roar_open(AudioOutput *ao, AudioFormat &audio_format, Error &error)
{
	RoarOutput *self = (RoarOutput *)ao;

	return self->Open(audio_format, error);
}

inline void
RoarOutput::Close()
{
	const ScopeLock protect(mutex);

	alive = false;

	if (vss != nullptr)
		roar_vs_close(vss, ROAR_VS_TRUE, &err);
	vss = nullptr;
	roar_disconnect(&con);
}

static void
roar_close(AudioOutput *ao)
{
	RoarOutput *self = (RoarOutput *)ao;
	self->Close();
}

inline void
RoarOutput::Cancel()
{
	const ScopeLock protect(mutex);

	if (vss == nullptr)
		return;

	roar_vs_t *_vss = vss;
	vss = nullptr;
	roar_vs_close(_vss, ROAR_VS_TRUE, &err);
	alive = false;

	_vss = roar_vs_new_from_con(&con, &err);
	if (_vss == nullptr)
		return;

	if (roar_vs_stream(_vss, &info, ROAR_DIR_PLAY, &err) < 0) {
		roar_vs_close(_vss, ROAR_VS_TRUE, &err);
		LogError(roar_output_domain, "Failed to start stream");
		return;
	}

	roar_vs_role(_vss, role, &err);
	vss = _vss;
	alive = true;
}

static void
roar_cancel(AudioOutput *ao)
{
	RoarOutput *self = (RoarOutput *)ao;

	self->Cancel();
}

inline size_t
RoarOutput::Play(const void *chunk, size_t size, Error &error)
{
	if (vss == nullptr) {
		error.Set(roar_output_domain, "Connection is invalid");
		return 0;
	}

	ssize_t nbytes = roar_vs_write(vss, chunk, size, &err);
	if (nbytes <= 0) {
		error.Set(roar_output_domain, "Failed to play data");
		return 0;
	}

	return nbytes;
}

static size_t
roar_play(AudioOutput *ao, const void *chunk, size_t size,
	  Error &error)
{
	RoarOutput *self = (RoarOutput *)ao;
	return self->Play(chunk, size, error);
}

static const char*
roar_tag_convert(TagType type, bool *is_uuid)
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
		case TAG_MUSICBRAINZ_RELEASETRACKID:
			*is_uuid = true;
			return "HASH";

		default:
			return nullptr;
	}
}

inline void
RoarOutput::SendTag(const Tag &tag)
{
	if (vss == nullptr)
		return;

	const ScopeLock protect(mutex);

	size_t cnt = 0;
	struct roar_keyval vals[32];
	char uuid_buf[32][64];

	char timebuf[16];
	if (!tag.duration.IsNegative()) {
		const unsigned seconds = tag.duration.ToS();
		snprintf(timebuf, sizeof(timebuf), "%02d:%02d:%02d",
			 seconds / 3600, (seconds % 3600) / 60, seconds % 60);

		vals[cnt].key = const_cast<char *>("LENGTH");
		vals[cnt].value = timebuf;
		++cnt;
	}

	for (const auto &item : tag) {
		if (cnt >= 32)
			break;

		bool is_uuid = false;
		const char *key = roar_tag_convert(item.type,
						   &is_uuid);
		if (key != nullptr) {
			vals[cnt].key = const_cast<char *>(key);

			if (is_uuid) {
				snprintf(uuid_buf[cnt], sizeof(uuid_buf[0]), "{UUID}%s",
					 item.value);
				vals[cnt].value = uuid_buf[cnt];
			} else {
				vals[cnt].value = const_cast<char *>(item.value);
			}

			cnt++;
		}
	}

	roar_vs_meta(vss, vals, cnt, &(err));
}

static void
roar_send_tag(AudioOutput *ao, const Tag *meta)
{
	RoarOutput *self = (RoarOutput *)ao;
	self->SendTag(*meta);
}

const struct AudioOutputPlugin roar_output_plugin = {
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
