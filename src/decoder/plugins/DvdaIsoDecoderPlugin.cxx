/*
 * Copyright (C) 2003-2017 The Music Player Daemon Project
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
#include <audio_stream.h>
#include <audio_track.h>
#include <stream_buffer.h>
#include <log_trunk.h>
#include <dvda_disc.h>
#include <dvda_metabase.h>
#include "DvdaIsoDecoderPlugin.hxx"
#include "../DecoderAPI.hxx"
#include "input/InputStream.hxx"
#include "CheckAudioFormat.hxx"
#include "tag/Handler.hxx"
#include "tag/Builder.hxx"
#include "DetachedSong.hxx"
#include "fs/Path.hxx"
#include "fs/AllocatedPath.hxx"
#include "thread/Cond.hxx"
#include "thread/Mutex.hxx"
#include "util/Alloc.hxx"
#include "util/bit_reverse.h"
#include "util/FormatString.hxx"
#include "util/AllocatedString.hxx"
#include "util/UriUtil.hxx"
#include "util/Domain.hxx"
#include "Log.hxx"

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

using namespace std;

static const char* DVDA_TRACKXXX_FMT = "AUDIO_TS__TRACK%03u%c.%3s";

static constexpr double SHORT_TRACK_SEC = 2.0;
static constexpr Domain dvdaiso_domain("dvdaiso");

static bool     param_no_downmixes;
static bool     param_no_short_tracks;
static chmode_t param_playable_area;
static string   param_tags_path;
static bool     param_tags_with_iso;

static string           dvda_uri;
static dvda_media_t*    dvda_media    = nullptr;
static dvda_reader_t*   dvda_reader   = nullptr;
static dvda_metabase_t* dvda_metabase = nullptr;

static unsigned
get_container_path_length(const char* path) {
	string container_path = path;
	container_path.resize(strrchr(container_path.c_str(), '/') - container_path.c_str());
	return container_path.length();
}

static string
get_container_path(const char* path) {
	string container_path = path;
	unsigned length = get_container_path_length(path);
	if (length >= 4) {
		container_path.resize(length);
		const char* c_str = container_path.c_str();
		if (strcasecmp(c_str + length - 4, ".iso") != 0) {
			container_path.resize(0);
		}
	}
	return container_path;
}

static bool
get_subsong(const char* path, unsigned* track_index, bool* downmix) {
	unsigned length = get_container_path_length(path);
	int params = 0;
	if (length > 0) {
		const char* ptr = path + length + 1;
		unsigned track_number;
		char area = '\0';
		char suffix[4];
		params = sscanf(ptr, DVDA_TRACKXXX_FMT, &track_number, &area, &suffix);
		suffix[3] = '\0';
		*track_index = track_number - 1;
		*downmix = area == 'D';
	}
	return params == 3;
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
	if (dvda_metabase != nullptr) {
		delete dvda_metabase;
		dvda_metabase = nullptr;
	}
	if (path != nullptr) {
		dvda_media = new dvda_media_stream_t();
		if (!dvda_media) {
			LogError(dvdaiso_domain, "new dvda_media_file_t() failed");
			dvda_uri.clear();
			return false;
		}
		dvda_reader = new dvda_disc_t();
		if (!dvda_reader) {
			LogError(dvdaiso_domain, "new dvda_disc_t() failed");
			dvda_uri.clear();
			return false;
		}
		if (!dvda_media->open(path)) {
			string err;
			err  = "dvda_media->open('";
			err += path;
			err += "') failed";
			LogWarning(dvdaiso_domain, err.c_str());
			dvda_uri.clear();
			return false;
		}
		if (!dvda_reader->open(dvda_media)) {
			//LogWarning(dvdaiso_domain, "dvda_reader->open(...) failed");
			dvda_uri.clear();
			return false;
		}
		if (!param_tags_path.empty() || param_tags_with_iso) {
			string tags_file;
			if (param_tags_with_iso) {
				tags_file = path;
				tags_file.resize(tags_file.rfind('.') + 1);
				tags_file.append("xml");
			}
			dvda_metabase = new dvda_metabase_t(reinterpret_cast<dvda_disc_t*>(dvda_reader), param_tags_path.empty() ? nullptr : param_tags_path.c_str(), tags_file.empty() ? nullptr : tags_file.c_str());
		}
	}
	dvda_uri = curr_uri;
	return true;
}

static void
dvdaiso_scan_info(unsigned track_index, bool downmix, const TagHandler& handler, void* handler_ctx) {
	string tag_value = to_string(track_index + 1);
	tag_handler_invoke_tag(handler, handler_ctx, TAG_TRACK, tag_value.c_str());
	tag_handler_invoke_duration(handler, handler_ctx, SongTime::FromS(dvda_reader->get_duration(track_index)));
	if (!dvda_metabase || (dvda_metabase && !dvda_metabase->get_info(track_index, downmix, handler, handler_ctx))) {
		dvda_reader->get_info(track_index, downmix, handler, handler_ctx);
	}
}

static bool
dvdaiso_init(const ConfigBlock& block) {
	my_av_log_set_callback(mpd_av_log_callback);
	param_no_downmixes = block.GetBlockValue("no_downmixes", true);
	param_no_short_tracks = block.GetBlockValue("no_short_tracks", true);
	const char* playable_area = block.GetBlockValue("playable_area", nullptr);
	param_playable_area = CHMODE_BOTH;
	if (playable_area != nullptr) {
		if (strcmp(playable_area, "stereo") == 0) {
			param_playable_area = CHMODE_TWOCH;
		}
		if (strcmp(playable_area, "multichannel") == 0) {
			param_playable_area = CHMODE_MULCH;
		}
	}
	param_tags_path = block.GetBlockValue("tags_path", "");
	param_tags_with_iso = block.GetBlockValue("tags_with_iso", false);
	return true;
}

static void
dvdaiso_finish() {
	dvdaiso_update_ifo(nullptr);
	my_av_log_set_default_callback();
}

static std::forward_list<DetachedSong>
dvdaiso_container_scan(Path path_fs) {
	std::forward_list<DetachedSong> list;
	if (!dvdaiso_update_ifo(path_fs.c_str())) {
		return list;
	}
	TagBuilder tag_builder;
	auto tail = list.before_begin();
	const char* suffix = uri_get_suffix(path_fs.c_str());
	char track_name[64];
	for (unsigned track_index = 0; track_index < dvda_reader->get_tracks(); track_index++) {
		if (dvda_reader->select_track(track_index)) {
			double duration = dvda_reader->get_duration();
			if (param_no_short_tracks && duration < SHORT_TRACK_SEC) {
				continue;
			}
			bool add_track = false;
			bool add_downmix = false;
			switch (param_playable_area) {
			case CHMODE_MULCH:
				if (dvda_reader->get_channels() > 2) {
					add_track = true;
				}
				break;
			case CHMODE_TWOCH:
				if (dvda_reader->get_channels() <= 2) {
					add_track = true;
				}
				if (!param_no_downmixes && dvda_reader->can_downmix()) {
					add_downmix = true;
				}
				break;
			default:
				add_track = true;
				if (!param_no_downmixes && dvda_reader->can_downmix()) {
					add_downmix = true;
				}
				break;
			}
			if (add_track) {
				dvdaiso_scan_info(track_index, false, add_tag_handler, &tag_builder);
				auto area = dvda_reader->get_channels() > 2 ? 'M' : 'S';
				sprintf(track_name, DVDA_TRACKXXX_FMT, track_index + 1, area, suffix);
				tail = list.emplace_after(tail, track_name, tag_builder.Commit());
			}
			if (add_downmix) {
				dvdaiso_scan_info(track_index, true, add_tag_handler, &tag_builder);
				auto area = 'D';
				sprintf(track_name, DVDA_TRACKXXX_FMT, track_index + 1, area, suffix);
				tail = list.emplace_after(tail, track_name, tag_builder.Commit());
			}
		}
		else {
			LogError(dvdaiso_domain, "cannot select track");
		}
	}
	return list;
}

static void
dvdaiso_file_decode(DecoderClient &client, Path path_fs) {
	string path_container = get_container_path(path_fs.c_str());
	if (!dvdaiso_update_ifo(path_container.c_str())) {
		return;
	}
	unsigned track;
	bool downmix;
	if (!get_subsong(path_fs.c_str(), &track, &downmix)) {
		LogError(dvdaiso_domain, "cannot get track number");
		return;
	}

	// initialize reader
	if (!dvda_reader->select_track(track)) {
		LogError(dvdaiso_domain, "cannot select track");
		return;
	}
	if (!dvda_reader->set_downmix(downmix)) {
		LogError(dvdaiso_domain, "cannot downmix track");
		return;
	}
	unsigned samplerate = dvda_reader->get_samplerate();
	unsigned channels = dvda_reader->get_downmix() ? 2 : dvda_reader->get_channels();
	vector<uint8_t> pcm_data(192000);

	// initialize decoder
	AudioFormat audio_format = CheckAudioFormat(samplerate, SampleFormat::S32, channels);
	SongTime songtime = SongTime::FromS(dvda_reader->get_duration(track));
	client.Ready(audio_format, true, songtime);
	auto frame_size = audio_format.GetFrameSize();

	// play
	auto cmd = client.GetCommand();
	for (;;) {
		size_t pcm_size = pcm_data.size();
		if (dvda_reader->read_frame(pcm_data.data(), &pcm_size)) {
			if (pcm_size > 0) {
				cmd = client.SubmitData(nullptr, pcm_data.data(), pcm_size, samplerate / 1000);
				if (cmd == DecoderCommand::STOP) {
					break;
				}
				if (cmd == DecoderCommand::SEEK) {
					double seconds = client.GetSeekTime().ToDoubleS();
					if (dvda_reader->seek(seconds)) {
						client.CommandFinished();
					}
					else {
						client.SeekError();
					}
					cmd = client.GetCommand();
				}
			}
		}
		else {
			break;
		}
	}
}

static bool
dvdaiso_scan_file(Path path_fs, const struct TagHandler& handler, void* handler_ctx) {
	string path_container = get_container_path(path_fs.c_str());
	if (path_container.empty()) {
		return false;
	}
	if (!dvdaiso_update_ifo(path_container.c_str())) {
		return false;
	}
	unsigned track_index;
	bool downmix;
	if (!get_subsong(path_fs.c_str(), &track_index, &downmix)) {
		LogError(dvdaiso_domain, "cannot get track number");
		return false;
	}
	dvdaiso_scan_info(track_index, downmix, handler, handler_ctx);
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
