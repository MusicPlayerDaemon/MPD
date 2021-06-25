/*
 * Copyright 2003-2021 The Music Player Daemon Project
 * http://www.musicpd.org
 * Copyright (C) 2014-2015 François 'mmu_man' Revol
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

#include "HaikuOutputPlugin.hxx"
#include "../OutputAPI.hxx"
#include "mixer/MixerList.hxx"
#include "util/Domain.hxx"
#include "util/Math.hxx"
#include "system/Error.hxx"
#include "Log.hxx"

#include <AppFileInfo.h>
#include <Application.h>
#include <Bitmap.h>
#include <IconUtils.h>
#include <MediaDefs.h>
#include <MediaRoster.h>
#include <Notification.h>
#include <OS.h>
#include <Resources.h>
#include <StringList.h>
#include <SoundPlayer.h>

#include <string.h>

#define UTF8_PLAY "\xE2\x96\xB6"

class HaikuOutput final: AudioOutput {
	friend int haiku_output_get_volume(HaikuOutput &haiku);
	friend bool haiku_output_set_volume(HaikuOutput &haiku, unsigned volume);

	size_t write_size;

	media_raw_audio_format format;
	BSoundPlayer* sound_player;

	sem_id new_buffer;
	sem_id buffer_done;

	uint8* buffer;
	size_t buffer_size;
	size_t buffer_filled;

	unsigned buffer_delay;

public:
	HaikuOutput(const ConfigBlock &block)
		:AudioOutput(0),
		 /* XXX: by default we should let the MediaKit propose the buffer size */
		 write_size(block.GetPositiveValue("write_size", 4096u)) {}

	~HaikuOutput();

	static AudioOutput *Create(EventLoop &event_loop,
				   const ConfigBlock &block);

private:
	void Open(AudioFormat &audio_format) override;
	void Close() noexcept override;

	size_t Play(const void *chunk, size_t size) override;

	std::chrono::steady_clock::duration Delay() const noexcept override;

	static void _FillBuffer(void* cookie, void* _buffer, size_t size,
		[[maybe_unused]] const media_raw_audio_format& _format);
	void FillBuffer(void* _buffer, size_t size,
		[[maybe_unused]] const media_raw_audio_format& _format);

	void SendTag(const Tag &tag) override;
};

static constexpr Domain haiku_output_domain("haiku_output");

static void
initialize_application()
{
	// required to send the notification with a bitmap
	// TODO: actually Run() it and handle B_QUIT_REQUESTED
	// TODO: use some locking?
	if (be_app == NULL) {
		LogDebug(haiku_output_domain, "creating be_app");
		new BApplication("application/x-vnd.MusicPD");
	}
}

static void
finalize_application()
{
	// TODO: use some locking?
	delete be_app;
	be_app = NULL;
	LogDebug(haiku_output_domain, "deleting be_app");
}

static bool
haiku_test_default_device(void)
{
	BSoundPlayer testPlayer;
	return testPlayer.InitCheck() == B_OK;

}

inline AudioOutput *
HaikuOutput::Create(EventLoop &, const ConfigBlock &block)
{
	initialize_application();

	return new HaikuOutput(block);
}

void
HaikuOutput::Close() noexcept
{
	sound_player->SetHasData(false);
	delete_sem(new_buffer);
	delete_sem(buffer_done);
	sound_player->Stop();
	delete sound_player;
	sound_player = nullptr;
}

HaikuOutput::~HaikuOutput()
{
	finalize_application();
}

void
HaikuOutput::_FillBuffer(void* cookie, void* buffer, size_t size,
	const media_raw_audio_format& format)
{
	HaikuOutput *ad = (HaikuOutput *)cookie;
	ad->FillBuffer(buffer, size, format);
}


void
HaikuOutput::FillBuffer(void* _buffer, size_t size,
	[[maybe_unused]] const media_raw_audio_format& _format)
{

	buffer = (uint8*)_buffer;
	buffer_size = size;
	buffer_filled = 0;
	bigtime_t start = system_time();
	release_sem(new_buffer);
	acquire_sem(buffer_done);
	bigtime_t w = system_time() - start;

	if (w > 5000LL) {
		FmtDebug(haiku_output_domain,
			"haiku:fill_buffer waited {}us", w);
	}

	if (buffer_filled < buffer_size) {
		memset(buffer + buffer_filled, 0,
			buffer_size - buffer_filled);
		FmtDebug(haiku_output_domain,
			 "haiku:fill_buffer filled {} size {} clearing remainder",
			 buffer_filled, buffer_size);
	}
}

void
HaikuOutput::Open(AudioFormat &audio_format)
{
	status_t err;
	format = media_multi_audio_format::wildcard;

	switch (audio_format.format) {
	case SampleFormat::S8:
		format.format = media_raw_audio_format::B_AUDIO_CHAR;
		break;

	case SampleFormat::S16:
		format.format = media_raw_audio_format::B_AUDIO_SHORT;
		break;

	case SampleFormat::S32:
		format.format = media_raw_audio_format::B_AUDIO_INT;
		break;

	case SampleFormat::FLOAT:
		format.format = media_raw_audio_format::B_AUDIO_FLOAT;
		break;

	default:
		/* fall back to float */
		audio_format.format = SampleFormat::FLOAT;
		format.format = media_raw_audio_format::B_AUDIO_FLOAT;
		break;
	}

	format.frame_rate = audio_format.sample_rate;
	format.byte_order = B_MEDIA_HOST_ENDIAN;
	format.channel_count = audio_format.channels;

	buffer_size = 0;

	if (write_size)
		format.buffer_size = write_size;
	else
		format.buffer_size = BMediaRoster::Roster()->AudioBufferSizeFor(
			format.channel_count, format.format,
			format.frame_rate, B_UNKNOWN_BUS) * 2;

	FmtDebug(haiku_output_domain,
		 "using haiku driver ad: bs: {} ws: {} "
		 "channels {} rate {} fmt {:08x} bs {}",
		 buffer_size, write_size,
		 format.channel_count, format.frame_rate,
		 format.format, format.buffer_size);

	sound_player = new BSoundPlayer(&format, "MPD Output",
		HaikuOutput::_FillBuffer, NULL, this);

	err = sound_player->InitCheck();
	if (err != B_OK) {
		delete sound_player;
		sound_player = NULL;
		throw MakeErrno(err, "BSoundPlayer::InitCheck() failed");
	}

	// calculate the allowable delay for the buffer (ms)
	buffer_delay = format.buffer_size;
	buffer_delay /= (format.format &
		media_raw_audio_format::B_AUDIO_SIZE_MASK);
	buffer_delay /= format.channel_count;
	buffer_delay *= 1000 / format.frame_rate;
	// half of the total buffer play time
	buffer_delay /= 2;
	FmtDebug(haiku_output_domain, "buffer delay: {} ms", buffer_delay);

	new_buffer = create_sem(0, "New buffer request");
	buffer_done = create_sem(0, "Buffer done");

	sound_player->SetVolume(1.0);
	sound_player->Start();
	sound_player->SetHasData(false);
}

size_t
HaikuOutput::Play(const void *chunk, size_t size)
{
	BSoundPlayer* const soundPlayer = sound_player;
	const uint8 *data = (const uint8 *)chunk;

	if (!soundPlayer->HasData())
		soundPlayer->SetHasData(true);
	acquire_sem(new_buffer);

	size_t bytesLeft = size;
	while (bytesLeft > 0) {
		if (buffer_filled == buffer_size) {
			// Request another buffer from BSoundPlayer
			release_sem(buffer_done);
			acquire_sem(new_buffer);
		}

		const size_t copyBytes = std::min(bytesLeft, buffer_size
			- buffer_filled);
		memcpy(buffer + buffer_filled, data,
			copyBytes);
		buffer_filled += copyBytes;
		data += copyBytes;
		bytesLeft -= copyBytes;
	}


	if (buffer_filled < buffer_size) {
		// Continue filling this buffer the next time this function is called
		release_sem(new_buffer);
	} else {
		// Buffer is full
		release_sem(buffer_done);
		//soundPlayer->SetHasData(false);
	}

	return size;
}

inline std::chrono::steady_clock::duration
HaikuOutput::Delay() const noexcept
{
	unsigned delay = buffer_filled ? 0 : buffer_delay;

	//FmtDebug(haiku_output_domain,
	//		"delay={}", delay / 2);
	// XXX: doesn't work
	//return (delay / 2) ? 1 : 0;
	(void)delay;

	return std::chrono::steady_clock::duration::zero();
}

void
HaikuOutput::SendTag(const Tag &tag)
{
	status_t err;

	/* lazily initialized */
	static BBitmap *icon = NULL;

	if (icon == NULL) {
		BAppFileInfo info;
		BResources resources;
		err = resources.SetToImage((const void *)&HaikuOutput::SendTag);
		BFile file(resources.File());
		err = info.SetTo(&file);
		icon = new BBitmap(BRect(0, 0, (float)B_LARGE_ICON - 1,
			(float)B_LARGE_ICON - 1), B_BITMAP_NO_SERVER_LINK, B_RGBA32);
		err = info.GetIcon(icon, B_LARGE_ICON);
		if (err != B_OK) {
			delete icon;
			icon = NULL;
		}
	}

	BNotification notification(B_INFORMATION_NOTIFICATION);

	BString messageId("mpd_");
	messageId << find_thread(NULL);
	notification.SetMessageID(messageId);

	notification.SetGroup("Music Player Daemon");

	char timebuf[16];
	unsigned seconds = 0;
	if (!tag.duration.IsNegative()) {
		seconds = tag.duration.ToS();
		snprintf(timebuf, sizeof(timebuf), "%02u:%02u:%02u",
			 seconds / 3600, (seconds % 3600) / 60, seconds % 60);
	}

	BString artist;
	BString album;
	BString title;
	BString track;
	BString name;

	for (const auto &item : tag)
	{
		switch (item.type) {
		case TAG_ARTIST:
		case TAG_ALBUM_ARTIST:
			if (artist.Length() == 0)
				artist << item.value;
			break;
		case TAG_ALBUM:
			if (album.Length() == 0)
				album << item.value;
			break;
		case TAG_TITLE:
			if (title.Length() == 0)
				title << item.value;
			break;
		case TAG_TRACK:
			if (track.Length() == 0)
				track << item.value;
			break;
		case TAG_NAME:
			if (name.Length() == 0)
				name << item.value;
			break;
		case TAG_GENRE:
		case TAG_DATE:
		case TAG_ORIGINAL_DATE:
		case TAG_PERFORMER:
		case TAG_COMMENT:
		case TAG_DISC:
		case TAG_COMPOSER:
		case TAG_MUSICBRAINZ_ARTISTID:
		case TAG_MUSICBRAINZ_ALBUMID:
		case TAG_MUSICBRAINZ_ALBUMARTISTID:
		case TAG_MUSICBRAINZ_TRACKID:
		default:
			FmtDebug(haiku_output_domain,
				 "tag item: type {} value '{}'\n",
				 item.type, item.value);
			break;
		}
	}

	notification.SetTitle(UTF8_PLAY " Now Playing:");

	BStringList content;
	if (name.Length())
		content.Add(name);
	if (artist.Length())
		content.Add(artist);
	if (album.Length())
		content.Add(album);
	if (track.Length())
		content.Add(track);
	if (title.Length())
		content.Add(title);

	if (content.CountStrings() == 0)
		content.Add("(Unknown)");

	BString full = content.Join(" " B_UTF8_BULLET " ");

	if (seconds > 0)
		full << " (" << timebuf << ")";

	notification.SetContent(full);

	err = notification.SetIcon(icon);

	notification.Send();
}

int
haiku_output_get_volume(HaikuOutput &haiku)
{
	BSoundPlayer* const soundPlayer = haiku.sound_player;

	if (soundPlayer == NULL || soundPlayer->InitCheck() != B_OK)
		return 0;

	return lround(soundPlayer->Volume() * 100);
}

bool
haiku_output_set_volume(HaikuOutput &haiku, unsigned volume)
{
	BSoundPlayer* const soundPlayer = haiku.sound_player;

	if (soundPlayer == NULL || soundPlayer->InitCheck() != B_OK)
		return false;

	soundPlayer->SetVolume((float)volume / 100);
	return true;
}

const struct AudioOutputPlugin haiku_output_plugin = {
	"haiku",
	haiku_test_default_device,
	&HaikuOutput::Create,
	&haiku_mixer_plugin,
};
