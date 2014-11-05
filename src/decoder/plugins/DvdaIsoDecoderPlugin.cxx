/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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
#include "audio_stream.h"
#include "audio_track.h"
#include "stream_buffer.h"
#include "log_trunk.h"
#include "dvda_disc.h"
#include "DvdaIsoDecoderPlugin.hxx"
#include "../DecoderAPI.hxx"
#include "input/InputStream.hxx"
#include "CheckAudioFormat.hxx"
#include "tag/TagHandler.hxx"
#include "fs/Path.hxx"
#include "thread/Cond.hxx"
#include "thread/Mutex.hxx"
#include "util/Alloc.hxx"
#include "util/bit_reverse.h"
#include "util/FormatString.hxx"
#include "util/UriUtil.hxx"
#include "util/Error.hxx"
#include "util/Domain.hxx"
#include "Log.hxx"

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

using namespace std;

#define DVDA_TRACKXXX_FMT "%cC_AUDIO__TRACK%03u.%s"

static constexpr Domain dvdaiso_domain("dvdaiso");

static bool      param_no_downmixes;
static bool      param_no_short_tracks;

static string         dvda_uri;
static dvda_media_t*  dvda_media  = nullptr;
static dvda_reader_t* dvda_reader = nullptr;

static unsigned
get_container_path_length(const char* path) {
	string container_path = path;
	container_path.resize(::strrchr(container_path.c_str(), '/') - container_path.c_str());
	return container_path.length();
}

static string
get_container_path(const char* path) {
	string path_container = path;
	unsigned length = get_container_path_length(path);
	if (length > 0) {
		path_container.resize(length);
	}
	return path_container;
}

static unsigned
get_subsong(const char* path) {
	unsigned length = get_container_path_length(path);
	if (length > 0) {
		const char* ptr = path + length + 1;
		char area = '\0';
		unsigned track = 0;
		char suffix[4];
		sscanf(ptr, DVDA_TRACKXXX_FMT, &area, &track, suffix);
		return track - 1;
	}
	return 0;
}

static bool
dvdaiso_update_ifo(const char* path) {
	string curr_uri = path;
	if (path != nullptr) {
		if (!dvda_uri.compare(curr_uri)) {
			return true;
		}
	}
	else {
		if (dvda_uri.empty()) {
			return true;
		}
	}
	if (dvda_reader != nullptr) {
		dvda_reader->close();
		delete dvda_reader;
		dvda_reader = nullptr;
	}
	if (dvda_media != nullptr) {
		dvda_media->close();
		delete dvda_media;
		dvda_media = nullptr;
	}
	if (path != nullptr) {
		dvda_media = new dvda_media_stream_t();
		if (!dvda_media) {
			LogError(dvdaiso_domain, "new dvda_media_file_t() failed");
			return false;
		}
		dvda_reader = new dvda_disc_t(param_no_downmixes, param_no_short_tracks);
		if (!dvda_reader) {
			LogError(dvdaiso_domain, "new dvda_disc_t() failed");
			return false;
		}
		if (!dvda_media->open(path)) {
			string err;
			err  = "dvda_media->open('";
			err += path;
			err += "') failed";
			LogWarning(dvdaiso_domain, err.c_str());
			return false;
		}
		if (!dvda_reader->open(dvda_media)) {
			LogWarning(dvdaiso_domain, "dvda_reader->open(...) failed");
			return false;
		}
	}
	dvda_uri = curr_uri;
	return true;
}

static bool
dvdaiso_init(const config_param& param) {
	my_av_log_set_callback(mpd_av_log_callback);
	param_no_downmixes = param.GetBlockValue("no_downmixes",  false);
	param_no_short_tracks = param.GetBlockValue("no_short_tracks", false);
	return true;
}

static void
dvdaiso_finish() {
	dvdaiso_update_ifo(nullptr);
	my_av_log_set_default_callback();
}

static char*
dvdaiso_container_scan(Path path_fs, const unsigned int tnum) {
	if (!dvdaiso_update_ifo(path_fs.c_str())) {
		return nullptr;
	}
	unsigned track = tnum - 1;
	if (!(track < dvda_reader->get_tracks()))
	{
		return nullptr;
	}
	if (!dvda_reader->select_track(track)) {
		LogError(dvdaiso_domain, "cannot select track");
		return nullptr;
	}
	char area = dvda_reader->get_channels() > 2 ? 'M' : '2';
	const char* suffix = uri_get_suffix(path_fs.c_str());
	return FormatNew(DVDA_TRACKXXX_FMT, area, track + 1, suffix);
}

static void
dvdaiso_file_decode(Decoder& decoder, Path path_fs) {
	string path_container = get_container_path(path_fs.c_str());
	if (!dvdaiso_update_ifo(path_container.c_str())) {
		return;
	}
	unsigned track = get_subsong(path_fs.c_str());

	// initialize reader
	if (!dvda_reader->select_track(track)) {
		LogError(dvdaiso_domain, "cannot select track");
		return;
	}
	unsigned samplerate = dvda_reader->get_samplerate();
	unsigned channels = dvda_reader->get_channels();
	vector<uint8_t> pcm_data(192000);

	// initialize decoder
	Error error;
	AudioFormat audio_format;
	if (!audio_format_init_checked(audio_format, samplerate, SampleFormat::S32, channels, error)) {
		LogError(error);
		return;
	}
	SongTime songtime = SongTime::FromS(dvda_reader->get_duration(track));
	decoder_initialized(decoder, audio_format, true, songtime);

	// play
	DecoderCommand cmd = decoder_get_command(decoder);
	for (;;) {
		size_t pcm_size = pcm_data.size();
		if (dvda_reader->read_frame(pcm_data.data(), &pcm_size)) {
			if (pcm_size > 0) {
				cmd = decoder_data(decoder, nullptr, pcm_data.data(), pcm_size, samplerate / 1000);
				if (cmd == DecoderCommand::STOP) {
					break;
				}
				if (cmd == DecoderCommand::SEEK) {
					double seconds = decoder_seek_time(decoder).ToDoubleS();
					if (dvda_reader->seek(seconds)) {
						decoder_command_finished(decoder);
					}
					else {
						decoder_seek_error(decoder);
					}
					cmd = decoder_get_command(decoder);
				}
			}
		}
		else {
			break;
		}
	}
	if (!dvda_reader->close()) {
		LogError(dvdaiso_domain, "cannot close stream reader");
	}
}

static bool
dvdaiso_scan_file(Path path_fs, const struct tag_handler* handler, void* handler_ctx) {
	string path_container = get_container_path(path_fs.c_str());
	if (!dvdaiso_update_ifo(path_container.c_str())) {
		return false;
	}
	unsigned track = get_subsong(path_fs.c_str());
	string tag_value = to_string(track + 1);
	tag_handler_invoke_tag(handler, handler_ctx, TAG_TRACK, tag_value.c_str());
	tag_handler_invoke_duration(handler, handler_ctx, SongTime::FromS(dvda_reader->get_duration(track)));
	dvda_reader->get_info(track, handler, handler_ctx);
	return true;
}

static const char* const dvdaiso_suffixes[] = {
	"iso",
	nullptr
};

static const char* const dvdaiso_mime_types[] = {
	"application/x-iso",
	nullptr
};


extern const struct DecoderPlugin dvdaiso_decoder_plugin;
const struct DecoderPlugin dvdaiso_decoder_plugin = {
	"dvdaiso",
	dvdaiso_init,
	dvdaiso_finish,
	nullptr,
	dvdaiso_file_decode,
	dvdaiso_scan_file,
	nullptr,
	dvdaiso_container_scan,
	dvdaiso_suffixes,
	dvdaiso_mime_types,
};
