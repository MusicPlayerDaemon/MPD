/*
 * Copyright 2003-2017 The Music Player Daemon Project
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
#include "util/Domain.hxx"
#include "Log.hxx"

#include <string>
#include <stdexcept>

/* libroar/services.h declares roar_service_stream::new - work around
   this C++ problem */
#define new _new
#include <roaraudio.h>
#undef new

class RoarOutput final : AudioOutput {
	const std::string host, name;

	roar_vs_t * vss;
	int err = ROAR_ERROR_NONE;
	const int role;
	struct roar_connection con;
	struct roar_audio_info info;
	mutable Mutex mutex;
	bool alive;

public:
	RoarOutput(const ConfigBlock &block);

	static AudioOutput *Create(EventLoop &, const ConfigBlock &block) {
		return new RoarOutput(block);
	}

	int GetVolume() const;
	void SetVolume(unsigned volume);

private:
	void Open(AudioFormat &audio_format) override;
	void Close() noexcept override;

	void SendTag(const Tag &tag) override;
	size_t Play(const void *chunk, size_t size) override;
	void Cancel() noexcept override;
};

static constexpr Domain roar_output_domain("roar_output");

gcc_pure
static int
GetConfiguredRole(const ConfigBlock &block) noexcept
{
	const char *role = block.GetBlockValue("role");
	return role != nullptr
		? roar_str2role(role)
		: ROAR_ROLE_MUSIC;
}

RoarOutput::RoarOutput(const ConfigBlock &block)
	:AudioOutput(0),
	 host(block.GetBlockValue("server", "")),
	 name(block.GetBlockValue("name", "MPD")),
	 role(GetConfiguredRole(block))
{
}

inline int
RoarOutput::GetVolume() const
{
	const std::lock_guard<Mutex> protect(mutex);

	if (vss == nullptr || !alive)
		return -1;

	float l, r;
	int error;
	if (roar_vs_volume_get(vss, &l, &r, &error) < 0)
		throw std::runtime_error(roar_vs_strerr(error));

	return (l + r) * 50;
}

int
roar_output_get_volume(RoarOutput &roar)
{
	return roar.GetVolume();
}

inline void
RoarOutput::SetVolume(unsigned volume)
{
	assert(volume <= 100);

	const std::lock_guard<Mutex> protect(mutex);
	if (vss == nullptr || !alive)
		throw std::runtime_error("closed");

	int error;
	float level = volume / 100.0;

	if (roar_vs_volume_mono(vss, level, &error) < 0)
		throw std::runtime_error(roar_vs_strerr(error));
}

void
roar_output_set_volume(RoarOutput &roar, unsigned volume)
{
	roar.SetVolume(volume);
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

void
RoarOutput::Open(AudioFormat &audio_format)
{
	const std::lock_guard<Mutex> protect(mutex);

	if (roar_simple_connect(&con,
				host.empty() ? nullptr : host.c_str(),
				name.c_str()) < 0)
		throw std::runtime_error("Failed to connect to Roar server");

	vss = roar_vs_new_from_con(&con, &err);

	if (vss == nullptr || err != ROAR_ERROR_NONE)
		throw std::runtime_error("Failed to connect to server");

	roar_use_audio_format(&info, audio_format);

	if (roar_vs_stream(vss, &info, ROAR_DIR_PLAY, &err) < 0)
		throw std::runtime_error("Failed to start stream");

	roar_vs_role(vss, role, &err);
	alive = true;
}

void
RoarOutput::Close() noexcept
{
	const std::lock_guard<Mutex> protect(mutex);

	alive = false;

	if (vss != nullptr)
		roar_vs_close(vss, ROAR_VS_TRUE, &err);
	vss = nullptr;
	roar_disconnect(&con);
}

void
RoarOutput::Cancel() noexcept
{
	const std::lock_guard<Mutex> protect(mutex);

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

size_t
RoarOutput::Play(const void *chunk, size_t size)
{
	if (vss == nullptr)
		throw std::runtime_error("Connection is invalid");

	ssize_t nbytes = roar_vs_write(vss, chunk, size, &err);
	if (nbytes <= 0)
		throw std::runtime_error("Failed to play data");

	return nbytes;
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
		case TAG_MUSICBRAINZ_RELEASETRACKID:
			*is_uuid = true;
			return "HASH";

		default:
			return nullptr;
	}
}

void
RoarOutput::SendTag(const Tag &tag)
{
	if (vss == nullptr)
		return;

	const std::lock_guard<Mutex> protect(mutex);

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

const struct AudioOutputPlugin roar_output_plugin = {
	"roar",
	nullptr,
	&RoarOutput::Create,
	&roar_mixer_plugin,
};
