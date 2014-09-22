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
#include <lib/sacdiso/sacd_media.h>
#include <lib/sacdiso/sacd_reader.h>
#include <lib/sacdiso/sacd_disc.h>
#include <lib/sacdiso/dst_decoder_mpd.h>
#undef MAX_CHANNELS
#include "SacdIsoDecoderPlugin.hxx"

#include "../DecoderAPI.hxx"
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

#include <glib.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

#define SACD_TRACKXXX_FMT "%cC_AUDIO__TRACK%03u.%s"

static constexpr Domain sacdiso_domain("sacdiso");

static constexpr unsigned DST_DECODER_THREADS = 8;

static unsigned param_dstdec_threads;
static bool     param_edited_master;
static bool     param_lsbitfirst;

static std::string    sacd_uri;
static sacd_media_t*  sacd_media  = nullptr;
static sacd_reader_t* sacd_reader = nullptr;

static const char* const sacdiso_suffixes[] = {
	"dat", "iso",
	nullptr
};

static unsigned
get_container_path_length(const char* path) {
	std::string container_path = path;
	container_path.resize(::strrchr(container_path.c_str(), '/') - container_path.c_str());
	return container_path.length();
}

static std::string
get_container_path(const char* path) {
	std::string path_container = path;
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
		const char *ptr = path + length + 1;
		char area = '\0';
		unsigned track = 0;
		char suffix[4];
		sscanf(ptr, SACD_TRACKXXX_FMT, &area, &track, suffix);
		if (area == 'M') {
			track += sacd_reader->get_tracks(AREA_TWOCH);
		}
		track--;
		return track;
	}
	return 0;
}

static bool
sacdiso_update_toc(const char* path) {
	std::string curr_uri = path;
	if (path != nullptr) {
		if (!sacd_uri.compare(curr_uri)) {
			return true;
		}
	}
	else {
		if (sacd_uri.empty()) {
			return true;
		}
	}
	if (sacd_reader != nullptr) {
		sacd_reader->close();
		delete sacd_reader;
		sacd_reader = nullptr;
	}
	if (sacd_media != nullptr) {
		sacd_media->close();
		delete sacd_media;
		sacd_media = nullptr;
	}
	if (path != nullptr) {
		sacd_media = new sacd_media_file_t();
		if (!sacd_media) {
			LogError(sacdiso_domain, "new sacd_media_file_t() failed");
			return false;
		}
		sacd_reader = new sacd_disc_t;
		if (!sacd_reader) {
			LogError(sacdiso_domain, "new sacd_disc_t() failed");
			return false;
		}
		if (!sacd_media->open(path)) {
			LogWarning(sacdiso_domain, "sacd_media->open() failed");
			return false;
		}
		if (!sacd_reader->open(sacd_media, (param_edited_master ? MODE_FULL_PLAYBACK : 0))) {
			LogWarning(sacdiso_domain, "sacd_reader->open() failed");
			return false;
		}
	}
	sacd_uri = curr_uri;
	return true;
}

static bool
sacdiso_init(const config_param& param) {
	param_dstdec_threads = param.GetBlockValue("dstdec_threads", DST_DECODER_THREADS);
	param_edited_master  = param.GetBlockValue("edited_master",  false);
	param_lsbitfirst     = param.GetBlockValue("lsbitfirst", false);
	return true;
}

static void
sacdiso_finish() {
	sacdiso_update_toc(nullptr);
}

static char*
sacdiso_container_scan(Path path_fs, const unsigned int tnum) {
	if (!sacdiso_update_toc(path_fs.c_str())) {
		return nullptr;
	}
	unsigned twoch_count = sacd_reader->get_tracks(AREA_TWOCH);
	unsigned mulch_count = sacd_reader->get_tracks(AREA_MULCH);
	unsigned track = tnum - 1;
	if (track < twoch_count) {
		sacd_reader->select_area(AREA_TWOCH);
	}
	else {
		track -= twoch_count;
		if (track < mulch_count) {
			sacd_reader->select_area(AREA_MULCH);
		}
		else {
			LogError(sacdiso_domain, "track index is out of range");
			return nullptr;
		}
	}
	char area = sacd_reader->get_channels() > 2 ? 'M' : '2';
	const char* suffix = uri_get_suffix(path_fs.c_str());
	return FormatNew(SACD_TRACKXXX_FMT, area, track + 1, suffix);
}

static void
bit_reverse_buffer(uint8_t* p, uint8_t* end) {
	for (; p < end; ++p) {
		*p = bit_reverse(*p);
	}
}

static void
sacdiso_file_decode(Decoder& decoder, Path path_fs) {
	std::string path_container = get_container_path(path_fs.c_str());
	if (!sacdiso_update_toc(path_container.c_str())) {
		return;
	}
	unsigned track = get_subsong(path_fs.c_str());

	/* initialize reader */
	sacd_reader->set_emaster(param_edited_master);
	unsigned twoch_count = sacd_reader->get_tracks(AREA_TWOCH);
	if (track < twoch_count) {
		if (!sacd_reader->select_track(track, AREA_TWOCH, 0)) {
			LogError(sacdiso_domain, "cannot select track in stereo area");
			return;
		}
	}
	else {
		track -= twoch_count;
		if (track < sacd_reader->get_tracks(AREA_MULCH)) {
			if (!sacd_reader->select_track(track, AREA_MULCH, 0)) {
				LogError(sacdiso_domain, "cannot select track in multichannel area");
				return;
			}
		}
	}
	int dsd_samplerate = sacd_reader->get_samplerate();
	int dsd_channels = sacd_reader->get_channels();
	int dsd_buf_size = dsd_samplerate / 8 / 75 * dsd_channels;
	int dst_buf_size = dsd_samplerate / 8 / 75 * dsd_channels;
	std::vector<uint8_t> dsd_buf;
	std::vector<uint8_t> dst_buf;
	dsd_buf.resize(param_dstdec_threads * dsd_buf_size);
	dst_buf.resize(param_dstdec_threads * dst_buf_size);

	/* initialize decoder */
	Error error;
	AudioFormat audio_format;
	if (!audio_format_init_checked(audio_format, dsd_samplerate / 8, SampleFormat::DSD, dsd_channels, error)) {
		LogError(error);
		return;
	}
	SongTime songtime = SongTime::FromS(sacd_reader->get_duration(track));
	decoder_initialized(decoder, audio_format, true, songtime);

	/* play */
	uint8_t* dsd_data;
	uint8_t* dst_data;
	size_t dsd_size = 0;
	size_t dst_size = 0;
	dst_decoder_t* dst_decoder = nullptr;
	DecoderCommand cmd = decoder_get_command(decoder);
	for (;;) {
		int slot_nr = dst_decoder ? dst_decoder->slot_nr : 0;
		dsd_data = dsd_buf.data() + dsd_buf_size * slot_nr;
		dst_data = dst_buf.data() + dst_buf_size * slot_nr;
		dst_size = dst_buf_size;
		frame_type_e frame_type;
		if (sacd_reader->read_frame(dst_data, &dst_size, &frame_type)) {
			if (dst_size > 0) {
				if (frame_type == FRAME_INVALID) {
					dst_size = dst_buf_size;
					memset(dst_data, 0xAA, dst_size);
				}
				if (frame_type == FRAME_DST) {
					if (!dst_decoder) {
						if (dst_decoder_create_mt(&dst_decoder, param_dstdec_threads) != 0) {
							LogError(sacdiso_domain, "dst_decoder_create_mt() failed");
							break;
						}
						if (dst_decoder_init_mt(dst_decoder, sacd_reader->get_channels(), sacd_reader->get_samplerate()) != 0) {
							LogError(sacdiso_domain, "dst_decoder_init_mt() failed");
							break;
						}
					}
					dst_decoder_decode_mt(dst_decoder, dst_data, dst_size, &dsd_data, &dsd_size);
				}
				else {
					dsd_data = dst_data;
					dsd_size = dst_size;
				}
				if (dsd_size > 0) {
					if (param_lsbitfirst) {
						bit_reverse_buffer(dsd_data, dsd_data + dsd_size);
					}
					cmd = decoder_data(decoder, nullptr, dsd_data, dsd_size, dsd_samplerate / 1000);
				}
			}
		}
		else {
			for (;;) {
				dst_data = nullptr;
				dst_size = 0;
				dsd_data = nullptr;
				dsd_size = 0;
				if (dst_decoder) {
					dst_decoder_decode_mt(dst_decoder, dst_data, dst_size, &dsd_data, &dsd_size);
				}
				if (dsd_size > 0) {
					if (param_lsbitfirst) {
						bit_reverse_buffer(dsd_data, dsd_data + dsd_size);
					}
					cmd = decoder_data(decoder, nullptr, dsd_data, dsd_size, dsd_samplerate / 1000);
					if (cmd == DecoderCommand::STOP || cmd == DecoderCommand::SEEK) {
						break;
					}
				}
				else {
					break;
				}
			}
			break;
		}
		if (cmd == DecoderCommand::STOP) {
			break;
		}
		if (cmd == DecoderCommand::SEEK) {
			double seconds = decoder_seek_time(decoder).ToDoubleS();
			if (sacd_reader->seek(seconds)) {
				if (dst_decoder) {
					dst_decoder_flush_mt(dst_decoder);
				}
				decoder_command_finished(decoder);
			}
			else {
				decoder_seek_error(decoder);
			}
			cmd = decoder_get_command(decoder);
		}
	}
	if (dst_decoder) {
		dst_decoder_free_mt(dst_decoder);
		dst_decoder_destroy_mt(dst_decoder);
		dst_decoder = nullptr;
	}
}

static bool
sacdiso_scan_file(Path path_fs, const struct tag_handler* handler, void* handler_ctx) {
	std::string path_container = get_container_path(path_fs.c_str());
	if (!sacdiso_update_toc(path_container.c_str())) {
		return false;
	}
	unsigned track = get_subsong(path_fs.c_str());
	unsigned twoch_count = sacd_reader->get_tracks(AREA_TWOCH);
	unsigned mulch_count = sacd_reader->get_tracks(AREA_MULCH);
	if (track < twoch_count) {
		sacd_reader->select_area(AREA_TWOCH);
	}
	else {
		track -= twoch_count;
		if (track < mulch_count) {
			sacd_reader->select_area(AREA_MULCH);
		}
		else {
			LogError(sacdiso_domain, "subsong index is out of range");
			return false;
		}
	}
	std::string tag_value = std::to_string(track + 1);
	tag_handler_invoke_tag(handler, handler_ctx, TAG_TRACK, tag_value.c_str());
	tag_handler_invoke_duration(handler, handler_ctx, SongTime::FromS(sacd_reader->get_duration(track)));
	sacd_reader->get_info(track, handler, handler_ctx);
	const char* track_format = sacd_reader->is_dst() ? "DST" : "DSD";
	tag_handler_invoke_pair(handler, handler_ctx, "codec", track_format);
	return true;
}

extern const struct DecoderPlugin sacdiso_decoder_plugin;
const struct DecoderPlugin sacdiso_decoder_plugin = {
	"sacdiso",
	sacdiso_init,
	sacdiso_finish,
	nullptr,
	sacdiso_file_decode,
	sacdiso_scan_file,
	nullptr,
	sacdiso_container_scan,
	sacdiso_suffixes,
	nullptr,
};
