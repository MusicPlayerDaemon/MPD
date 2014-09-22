/*
* SACD Decoder plugin
* Copyright (c) 2011-2013 Maxim V.Anisiutkin <maxim.anisiutkin@gmail.com>
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU Lesser General Public
* License as published by the Free Software Foundation; either
* version 2.1 of the License, or (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
* Lesser General Public License for more details.
*
* You should have received a copy of the GNU Lesser General Public
* License along with FFmpeg; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
*/

#include "../ATLHelpers/ATLHelpers.h"
#include "resource.h"

#include <stdint.h>
#include <foobar2000.h>

#include <component.h>
#include "dsd_source.h"
#include "sacd_reader.h"
#include "sacd_disc.h"
#include "sacd_dsdiff.h"
#include "sacd_dsf.h"
#include "sacd_metabase.h"
#include "sacd_setup.h"
#include "dst_decoder_foo.h"
#include "dsdpcm_converter.h"

using namespace pfc;

enum media_type_t {UNK_TYPE = 0, ISO_TYPE = 1, DSDIFF_TYPE = 2, DSF_TYPE = 3};

static const uint32_t UPDATE_STATS_MS = 500;
static const int BITRATE_AVGS = 16;

void console_fprintf(FILE* file, const char* fmt, ...) {
	va_list vl;
	va_start(vl, fmt);
	console::printfv(fmt, vl);
	va_end(vl);
}

void console_vfprintf(FILE* file, const char* fmt, va_list vl) {
	console::printfv(fmt, vl);
}

int get_sacd_channel_map_from_loudspeaker_config(int loudspeaker_config) {
	int sacd_channel_map;
	switch (loudspeaker_config) {
	case 0:
		sacd_channel_map = audio_chunk::channel_front_left | audio_chunk::channel_front_right;
		break;
	case 1:
		sacd_channel_map = audio_chunk::channel_front_left | audio_chunk::channel_front_right | audio_chunk::channel_back_left | audio_chunk::channel_back_right;
		break;
	case 2:
		sacd_channel_map = audio_chunk::channel_front_left | audio_chunk::channel_front_right | audio_chunk::channel_front_center | audio_chunk::channel_lfe;
		break;
	case 3:
		sacd_channel_map = audio_chunk::channel_front_left | audio_chunk::channel_front_right | audio_chunk::channel_front_center | audio_chunk::channel_back_left | audio_chunk::channel_back_right;
		break;
	case 4:
		sacd_channel_map = audio_chunk::channel_front_left | audio_chunk::channel_front_right | audio_chunk::channel_front_center | audio_chunk::channel_lfe | audio_chunk::channel_back_left | audio_chunk::channel_back_right;
		break;
	case 5:
		sacd_channel_map = audio_chunk::channel_front_center;
		break;
	case 6:
		sacd_channel_map = audio_chunk::channel_front_left | audio_chunk::channel_front_right | audio_chunk::channel_front_center;
		break;
	default:
		sacd_channel_map = 0;
		break;
	}
	return sacd_channel_map;
}

int get_sacd_channel_map_from_channels(int channels) {
	int sacd_channel_map;
	switch (channels) {
	case 2:
		sacd_channel_map = audio_chunk::channel_front_left | audio_chunk::channel_front_right;
		break;
	case 5:
		sacd_channel_map = audio_chunk::channel_front_left | audio_chunk::channel_front_right | audio_chunk::channel_front_center | audio_chunk::channel_back_left | audio_chunk::channel_back_right;
		break;
	case 6:
		sacd_channel_map = audio_chunk::channel_front_left | audio_chunk::channel_front_right | audio_chunk::channel_front_center | audio_chunk::channel_lfe | audio_chunk::channel_back_left | audio_chunk::channel_back_right;
		break;
	default:
		sacd_channel_map = 0;
		break;
	}
	return sacd_channel_map;
}

dsd_source_t       g_dsd_source;
dsdpcm_converter_t g_dsdpcm_playback;
bool               g_track_completed;
bool               g_cue_playback = false;

class playback_start : public main_thread_callback {
public:
	playback_start() {}
	void callback_run() {
		static_api_ptr_t<playback_control> pc;
		pc->start();
	}
};

class playback_stop : public main_thread_callback {
public:
	playback_stop() {}
	void callback_run() {
		static_api_ptr_t<playback_control> pc;
		pc->stop();
	}
};

class playback_handler_t : public play_callback_static {
	int  channels;
	int  samplerate;
	bool dsd_mode;
public:
	playback_handler_t() {
		channels   = 0;
		samplerate = 0;
		dsd_mode   = false;
	}
	void on_playback_starting(play_control::t_track_command p_command, bool p_paused) {}
	void on_playback_new_track(metadb_handle_ptr p_track) {
		int  new_channels;
		int  new_samplerate;
		bool new_dsd_mode;
		bool need_restart;
		file_info_impl info;
		new_channels = 0;
		new_samplerate = 0;
		if (p_track->get_info(info)) {
			new_channels   = (int)info.info_get_int("channels");
			new_samplerate = (int)info.info_get_int("samplerate");
			if (new_samplerate == 0) {
				new_samplerate = (int)info.info_get_int("original_samplerate");
			}
		}
		new_dsd_mode = CSACDPreferences::in_dsd_mode();
		need_restart = samplerate == DSDxFs1 && new_samplerate >= DSDxFs64 || samplerate >= DSDxFs64 && new_samplerate == DSDxFs1 || samplerate >= DSDxFs64 && new_samplerate >= DSDxFs64 && samplerate != new_samplerate;
		need_restart = need_restart || (samplerate > 0 && dsd_mode != new_dsd_mode);
		channels   = new_channels;
		samplerate = new_samplerate;
		dsd_mode   = new_dsd_mode;
		if (need_restart) {
			static_api_ptr_t<main_thread_callback_manager>()->add_callback(new service_impl_t<playback_stop>());
		}
		g_dsd_source.command(dsdcmd_t(dsdcmd_playback_start, channels, samplerate, new_samplerate >= DSDxFs64 && dsd_mode));
		if (need_restart) {
			static_api_ptr_t<main_thread_callback_manager>()->add_callback(new service_impl_t<playback_start>());
		}
	}
	void on_playback_stop(play_control::t_stop_reason p_reason) {
		g_dsd_source.command(dsdcmd_t(dsdcmd_playback_stop));
	}
	void on_playback_seek(double p_time) {}
	void on_playback_pause(bool p_state) {}
	void on_playback_edited(metadb_handle_ptr p_track) {}
	void on_playback_dynamic_info(const file_info& p_info) {}
	void on_playback_dynamic_info_track(const file_info& p_info) {}
	void on_playback_time(double p_time) {}
	void on_volume_change(float p_new_val) {}
	unsigned get_flags() {
		return flag_on_playback_new_track | flag_on_playback_stop;
	}
};

static play_callback_static_factory_t<playback_handler_t> g_playback_sacd_factory;

class input_sacd_t {
	media_type_t          media_type;
	sacd_media_t*         sacd_media;
	sacd_reader_t*        sacd_reader;
	sacd_metabase_t*      sacd_metabase;
	area_id_e             area_id;
	unsigned              flags;
	int                   sacd_bitrate[BITRATE_AVGS];
	int                   sacd_bitrate_idx;
	int                   sacd_bitrate_sum;
	array_t<uint8_t>      dsd_buf;
	int                   dsd_buf_size;
	array_t<uint8_t>      dst_buf;
	int                   dst_buf_size;
	array_t<audio_sample> pcm_buf;
	dst_decoder_t*        dst_decoder;
	dsdpcm_converter_t    dsdpcm_convert;
	uint32_t              info_update_time_ms;
	int                   pcm_out_channels;
	unsigned int          pcm_out_channel_map;
	int                   pcm_out_samplerate;
	int                   pcm_out_bits_per_sample;
	int                   pcm_out_samples;
	float                 pcm_out_delay;
	bool                  use_dsd_path;
	int                   dsd_samplerate;
	bool                  track_completed;
	bool                  cue_playback;
	int                   excpt_cnt;
public:
	input_sacd_t() {
		sacd_media = NULL;
		sacd_reader = NULL;
		dst_decoder = NULL;
		info_update_time_ms = 0;
		sacd_reader = NULL;
		sacd_metabase = NULL;
		cue_playback = false;
	}

	virtual ~input_sacd_t() {
		if (sacd_reader) {
			delete sacd_reader;
		}
		if (sacd_media) {
			delete sacd_media;
		}
		if (sacd_metabase) {
			delete sacd_metabase;
		}
		if (dst_decoder) {
			dst_decoder_destroy_mt(dst_decoder);
		}
	}

	void open(service_ptr_t<file> p_filehint, const char* p_path, t_input_open_reason p_reason, abort_callback& p_abort) {
		cue_playback = cue_playback || g_cue_playback;
		if (p_reason == input_open_decode) {
			g_cue_playback = false;
		}
		string_filename_ext filename_ext(p_path);
		string_extension ext(p_path);
		bool raw_media  = false;
		media_type = UNK_TYPE;
		if (stricmp_utf8(ext, "ISO") == 0) {
			media_type = ISO_TYPE;
		} else if (stricmp_utf8(ext, "DAT") == 0) {
			media_type = ISO_TYPE;
		} else if (stricmp_utf8(ext, "DFF") == 0) {
			media_type = DSDIFF_TYPE;
		} else if (stricmp_utf8(ext, "DSF") == 0) {
			media_type = DSF_TYPE;
		} else if ((stricmp_utf8(filename_ext, "") == 0 || stricmp_utf8(filename_ext, "MASTER1.TOC") == 0) && strlen_utf8(p_path) > 7 && sacd_disc_t::g_is_sacd(p_path[7])) {
			media_type = ISO_TYPE;
			raw_media = true;
		}
		if (media_type == UNK_TYPE) {
			throw exception_io_unsupported_format();
		}
		if (raw_media) {
			sacd_media = new sacd_media_disc_t();
			if (!sacd_media) {
				throw exception_overflow();
			}
		}
		else {
			sacd_media = new sacd_media_file_t();
			if (!sacd_media) {
				throw exception_overflow();
			}
		}
		switch (media_type) {
		case ISO_TYPE:
			sacd_reader = new sacd_disc_t;
			if (!sacd_reader) {
				throw exception_overflow();
			}
			break;
		case DSDIFF_TYPE:
			sacd_reader = new sacd_dsdiff_t;
			if (!sacd_reader) {
				throw exception_overflow();
			}
			break;
		case DSF_TYPE:
			sacd_reader = new sacd_dsf_t;
			if (!sacd_reader) {
				throw exception_overflow();
			}
			break;
		default:
			throw exception_io_data();
			break;
		}
		if (!sacd_media->open(p_filehint, p_path, p_reason)) {
			throw exception_io_data();
		}
		if (!sacd_reader->open(sacd_media, (cue_playback ? MODE_SINGLE_TRACK : 0) | (CSACDPreferences::get_emaster() ? MODE_FULL_PLAYBACK : 0))) {
			throw exception_io_data();
		}
		switch (media_type) {
		case ISO_TYPE:
			if (CSACDPreferences::get_editable_tags()) {
				const char* metafile_path = NULL;
				string_replace_extension metafile_name(p_path, "xml");
				if (!raw_media && CSACDPreferences::get_store_tags_with_iso()) {
					metafile_path = metafile_name;
				}
				sacd_metabase = new sacd_metabase_t(reinterpret_cast<sacd_disc_t*>(sacd_reader), metafile_path);
			}
			break;
		}
		pcm_out_samplerate = CSACDPreferences::get_samplerate();
		pcm_out_bits_per_sample = 24;
	}

	t_uint32 get_subsong_count() {
		area_id = (area_id_e)CSACDPreferences::get_area();
		t_uint32 track_count = 0;
		switch (area_id) {
		case AREA_TWOCH:
			track_count = sacd_reader->get_track_count(AREA_TWOCH);
			if (track_count == 0) {
				area_id = AREA_BOTH;
				track_count = sacd_reader->get_track_count(AREA_MULCH);
			}
			break;
		case AREA_MULCH:
			track_count = sacd_reader->get_track_count(AREA_MULCH);
			if (track_count == 0) {
				area_id = AREA_BOTH;
				track_count = sacd_reader->get_track_count(AREA_TWOCH);
			}
			break;
		default:
			track_count = sacd_reader->get_track_count(AREA_TWOCH) + sacd_reader->get_track_count(AREA_MULCH);
			break;
		}
		return track_count;
	}

	t_uint32 get_subsong(t_uint32 p_index) {
		switch (area_id) {
		case AREA_MULCH:
			p_index += sacd_reader->get_track_count(AREA_TWOCH);
			break;
		}
		return p_index;
	}

	void get_info(t_uint32 p_subsong, file_info& p_info, abort_callback& p_abort) {
		t_uint32 subsong = p_subsong;
		if (media_type == ISO_TYPE) {
			uint8_t twoch_count = sacd_reader->get_track_count(AREA_TWOCH);
			if (subsong < twoch_count) {
				sacd_reader->set_area(AREA_TWOCH);
			}
			else {
				subsong -= twoch_count;
				if (subsong < sacd_reader->get_track_count(AREA_MULCH)) {
					sacd_reader->set_area(AREA_MULCH);
				}
			}
		}
		p_info.set_length(sacd_reader->get_duration(subsong));
		p_info.info_set_int("samplerate", sacd_reader->get_samplerate());
		p_info.info_set_int("channels", sacd_reader->get_channels());
		p_info.info_set_int("bitspersample", pcm_out_bits_per_sample);
		string_formatter(codec);
		codec << (sacd_reader->is_dst() ? "DST" : "DSD");
		codec << sacd_reader->get_samplerate() / DSDxFs1;
		p_info.info_set("codec", codec);
		p_info.info_set("encoding", "lossless");
		p_info.info_set_bitrate(((t_int64)(sacd_reader->get_samplerate() * sacd_reader->get_channels()) + 500) / 1000);
		sacd_reader->get_info(subsong, p_info);
		if (sacd_metabase) {
			sacd_metabase->set_replaygain((float)CSACDPreferences::get_volume());
			sacd_metabase->get_meta_info(p_subsong, p_info);
		}
	}

	t_filestats get_file_stats(abort_callback& p_abort) {
		return sacd_media->get_stats();
	}

	void decode_initialize(t_uint32 p_subsong, unsigned p_flags, abort_callback& p_abort) {
		t_uint32 subsong = p_subsong;
		flags = p_flags;
		sacd_reader->set_emaster(CSACDPreferences::get_emaster());
		uint8_t twoch_count = sacd_reader->get_track_count(AREA_TWOCH);
		if (subsong < twoch_count) {
			if (!sacd_reader->set_track(subsong, AREA_TWOCH, 0)) {
				throw exception_io();
			}
		}
		else {
			subsong -= twoch_count;
			if (subsong < sacd_reader->get_track_count(AREA_MULCH)) {
				if (!sacd_reader->set_track(subsong, AREA_MULCH, 0)) {
					throw exception_io();
				}
			}
		}
		dsd_samplerate = sacd_reader->get_samplerate();
		pcm_out_channels = sacd_reader->get_channels();
		dst_buf_size = dsd_buf_size = dsd_samplerate / 8 / 75 * pcm_out_channels; 
		dsd_buf.set_size(DST_DECODER_THREADS * dsd_buf_size);
		dst_buf.set_size(DST_DECODER_THREADS * dst_buf_size);
		pcm_out_channel_map = get_sacd_channel_map_from_loudspeaker_config(sacd_reader->get_loudspeaker_config());
		pcm_out_channel_map = get_sacd_channel_map_from_channels(pcm_out_channels);
		pcm_out_samples = pcm_out_samplerate / 75;
		pcm_buf.set_size(pcm_out_channels * pcm_out_samples);
		memset(sacd_bitrate, 0, sizeof(sacd_bitrate));
		sacd_bitrate_idx = 0;
		sacd_bitrate_sum = 0;
		use_dsd_path = false;
		if (flags & input_flag_playback) {
			int rv = g_dsdpcm_playback.init(pcm_out_channels, dsd_samplerate, pcm_out_samplerate, (conv_type_t)CSACDPreferences::get_converter_mode(), CSACDPreferences::get_user_fir().get_ptr(), CSACDPreferences::get_user_fir().get_size(), g_track_completed);
			if (rv < 0) {
				if (rv == -2) {
					popup_message::g_show("No instaled FIR, continue with default", "DSD2PCM", popup_message::icon_error);
				}
				int rv = g_dsdpcm_playback.init(pcm_out_channels, dsd_samplerate, pcm_out_samplerate, DSDPCM_CONV_MULTISTAGE_SINGLE, NULL, 0, g_track_completed);
				if (rv < 0) {
					throw exception_io();
				}
			}
			g_dsdpcm_playback.set_gain((float)CSACDPreferences::get_volume());
			pcm_out_delay = g_dsdpcm_playback.get_delay();
			g_track_completed = false;
			use_dsd_path = CSACDPreferences::in_dsd_mode();
			g_dsd_source.command(dsdcmd_t(dsdcmd_decode_init, pcm_out_channels, dsd_samplerate, use_dsd_path));
		}
		else {
			int rv = dsdpcm_convert.init(pcm_out_channels, dsd_samplerate, pcm_out_samplerate, (conv_type_t)CSACDPreferences::get_converter_mode(), CSACDPreferences::get_user_fir().get_ptr(), CSACDPreferences::get_user_fir().get_size());
			if (rv < 0) {
				if (rv == -2) {
					popup_message::g_show("No installed FIR, continue with default", "DSD2PCM", popup_message::icon_error);
				}
				int rv = g_dsdpcm_playback.init(pcm_out_channels, dsd_samplerate, pcm_out_samplerate, DSDPCM_CONV_MULTISTAGE_SINGLE, NULL, 0, g_track_completed);
				if (rv < 0) {
					throw exception_io();
				}
			}
			dsdpcm_convert.set_gain((float)CSACDPreferences::get_volume());
			pcm_out_delay = dsdpcm_convert.get_delay();
		}
		track_completed = false;
		excpt_cnt = 0;
	}

	bool decode_run_internal(audio_chunk& p_chunk, abort_callback& p_abort) {
		uint8_t* dsd_data;
		uint8_t* dst_data;
		int pcm_samples;
		if (track_completed) {
			g_dsd_source.command(dsdcmd_t(dsdcmd_decode_free));
			return false;
		}
		size_t dsd_size = 0;
		size_t dst_size = 0;
		for (;;) {
			int slot_nr = dst_decoder ? dst_decoder->slot_nr : 0;
			dsd_data = dsd_buf.get_ptr() + dsd_buf_size * slot_nr;
			dst_data = dst_buf.get_ptr() + dst_buf_size * slot_nr;
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
							if (dst_decoder_create_mt(&dst_decoder, DST_DECODER_THREADS) != 0 || dst_decoder_init_mt(dst_decoder, sacd_reader->get_channels(), sacd_reader->get_samplerate()) != 0) {
								return false;
							}
						}
						dst_decoder_decode_mt(dst_decoder, dst_data, dst_size, &dsd_data, &dsd_size);
					}
					else {
						dsd_data = dst_data;
						dsd_size = dst_size;
					}
					sacd_bitrate_idx = (++sacd_bitrate_idx) % BITRATE_AVGS;
					sacd_bitrate_sum -= sacd_bitrate[sacd_bitrate_idx];
					sacd_bitrate[sacd_bitrate_idx] = dst_size * 8 * 75;
					sacd_bitrate_sum += sacd_bitrate[sacd_bitrate_idx];
					if (dsd_size > 0) {
						if (use_dsd_path) {
							g_dsd_source.write(pcm_out_channels, dsd_data, dsd_size);
							p_chunk.set_sample_rate(DSDxFs1);
							p_chunk.set_channels(pcm_out_channels, pcm_out_channel_map);
							p_chunk.set_silence(DSDxFs1 / 75);
						}
						else {
							int remove_samples = 0;
							if (flags & input_flag_playback) {
								if (!g_dsdpcm_playback.is_convert_called()) {
									remove_samples = (int)ceil(pcm_out_delay) + 1;
									pcm_samples = g_dsdpcm_playback.convert(dsd_data, pcm_buf.get_ptr(), dsd_size);
									g_dsdpcm_playback.degibbs(pcm_buf.get_ptr(), pcm_samples, 0); 
								}
								else {
									g_dsdpcm_playback.convert(dsd_data, pcm_buf.get_ptr(), dsd_size);
								}
							}
							else {
								if (!dsdpcm_convert.is_convert_called()) {
									remove_samples = (int)ceil(pcm_out_delay) + 1;
									pcm_samples = dsdpcm_convert.convert(dsd_data, pcm_buf.get_ptr(), dsd_size);
									dsdpcm_convert.degibbs(pcm_buf.get_ptr(), pcm_samples, 0); 
								}
								else {
									dsdpcm_convert.convert(dsd_data, pcm_buf.get_ptr(), dsd_size);
								}
							}
							if (remove_samples > 0) {
								p_chunk.set_data(pcm_buf.get_ptr() + pcm_out_channels * remove_samples, pcm_out_samples - remove_samples, pcm_out_channels, pcm_out_samplerate, pcm_out_channel_map);
							}
							else {
								p_chunk.set_data(pcm_buf.get_ptr(), pcm_out_samples, pcm_out_channels, pcm_out_samplerate, pcm_out_channel_map);
							}
						}
						return true;
					}
				}
			}
			else {
				break;
			}
		}
		dsd_data = NULL;
		dst_data = NULL;
		dst_size = 0;
		if (dst_decoder) {
			dst_decoder_decode_mt(dst_decoder, dst_data, dst_size, &dsd_data, &dsd_size);
		}
		if (use_dsd_path) {
			if (dsd_size > 0) {
				g_dsd_source.write(pcm_out_channels, dsd_data, dsd_size);
				p_chunk.set_sample_rate(DSDxFs1);
				p_chunk.set_channels(pcm_out_channels, pcm_out_channel_map);
				p_chunk.set_silence(DSDxFs1 / 75);
				return true;
			}
			if (flags & input_flag_playback) {
				g_track_completed = true;
			}
		}
		else {
			if (dsd_size > 0) {
				if (flags & input_flag_playback) {
					g_dsdpcm_playback.convert(dsd_data, pcm_buf.get_ptr(), dsd_size);
				}
				else {
					dsdpcm_convert.convert(dsd_data, pcm_buf.get_ptr(), dsd_size);
				}
				p_chunk.set_data(pcm_buf.get_ptr(), pcm_out_samples, pcm_out_channels, pcm_out_samplerate, pcm_out_channel_map);
				return true;
			}
			if (flags & input_flag_playback) {
				g_track_completed = true;
			}
			else {
				dsd_data = dsd_buf.get_ptr();
				dsd_size = dsd_buf_size;
				memset(dsd_data, 0xAA, dsd_size);
				pcm_samples = dsdpcm_convert.convert(dsd_data, pcm_buf.get_ptr(), dsd_size);
				dsdpcm_convert.degibbs(pcm_buf.get_ptr(), pcm_samples, 1); 
				p_chunk.set_data(pcm_buf.get_ptr(), (int)floor(pcm_out_delay) - 1, pcm_out_channels, pcm_out_samplerate, pcm_out_channel_map);
				track_completed = true;
				return true;
			}
			track_completed = true;
		}
		g_dsd_source.command(dsdcmd_t(dsdcmd_decode_free));
		return false;
	}

	bool decode_run(audio_chunk& p_chunk, abort_callback& p_abort) {
		bool rv;
		__try {
			rv = decode_run_internal(p_chunk, p_abort);
			excpt_cnt = 0;
		}
		__except (EXCEPTION_EXECUTE_HANDLER) {
			excpt_cnt++;
			console::printf("Exception caught in decode_run");
			rv = excpt_cnt < 1000;
		}
		return rv;
	}

	void decode_seek(double p_seconds, abort_callback& p_abort) {
		if (use_dsd_path) {
			g_dsd_source.command(dsdcmd_t(dsdcmd_playback_seek));
		}
		if (!sacd_reader->seek(p_seconds)) {
			throw exception_io();
		}
	}

	bool decode_can_seek() {
		return sacd_media->can_seek();
	}

	bool decode_get_dynamic_info(file_info& p_info, double& p_timestamp_delta) {
		DWORD curr_time_ms = GetTickCount();
		if (info_update_time_ms + (DWORD)UPDATE_STATS_MS >= curr_time_ms) {
			return false;
		}
		info_update_time_ms = curr_time_ms;
		p_info.info_set_bitrate_vbr(((t_int64)(sacd_bitrate_sum / BITRATE_AVGS) + 500) / 1000);
		string_formatter(codec);
		codec << (sacd_reader->is_dst() ? "DST" : "DSD");
		codec << sacd_reader->get_samplerate() / DSDxFs1;
		p_info.info_set("codec", codec);
		if (use_dsd_path) {
			p_info.info_set_int("samplerate", sacd_reader->get_samplerate());
		}
		else {
			p_info.info_set_int("samplerate", pcm_out_samplerate);
		}
		return true;
	}
	
	bool decode_get_dynamic_info_track(file_info& p_info, double& p_timestamp_delta) {
		return false;
	}
	
	void decode_on_idle(abort_callback & p_abort) {
		sacd_media->on_idle();
	}

	void retag_set_info(t_uint32 p_subsong, const file_info& p_info, abort_callback& p_abort) {
		if (CSACDPreferences::get_editable_tags() && !cue_playback) {
			if (sacd_metabase) {
				sacd_metabase->set_replaygain((float)CSACDPreferences::get_volume());
				sacd_metabase->set_meta_info(p_subsong, p_info);
			}
			sacd_reader->set_info(p_subsong, p_info);
		}
	}
	
	void retag_commit(abort_callback& p_abort) {
		if (CSACDPreferences::get_editable_tags() && !cue_playback) {
			if (sacd_metabase) {
				sacd_metabase->commit();
			}
			sacd_reader->commit();
		}
	}

	static bool g_is_our_content_type(const char* p_content_type) {
		return false;
	}
	
	static bool g_is_our_path(const char* p_path, const char* p_ext) {
		g_cue_playback = g_cue_playback || stricmp_utf8(p_ext, "CUE") == 0; 
		string_filename_ext filename_ext(p_path);
		return
			(stricmp_utf8(p_ext, "ISO") == 0 || stricmp_utf8(p_ext, "DAT") == 0) && sacd_disc_t::g_is_sacd(p_path) ||
			stricmp_utf8(p_ext, "DFF") == 0 ||
			stricmp_utf8(p_ext, "DSF") == 0 ||
			(stricmp_utf8(filename_ext, "") == 0 || stricmp_utf8(filename_ext, "MASTER1.TOC") == 0) && strlen_utf8(p_path) > 7 && sacd_disc_t::g_is_sacd(p_path[7]);
	}
};

static input_factory_t<input_sacd_t> g_input_sacd_factory;

class initquit_sacd_t : public initquit {
public:
	virtual void on_init() {
		g_dsd_source.command(dsdcmd_t(dsdcmd_source_init));
	}
	
	virtual void on_quit() {
		g_dsd_source.command(dsdcmd_t(dsdcmd_source_free));
	}
};

static initquit_factory_t<initquit_sacd_t> g_initquit_sacd_factory;

DECLARE_COMPONENT_VERSION("Super Audio CD Decoder", "0.7.1", "Super Audio CD Decoder Input PlugIn.\n\nCopyright (c) 2011-2014 Maxim V.Anisiutkin <maxim.anisiutkin@gmail.com>");
DECLARE_FILE_TYPE("SACD files", "*.DAT;*.DFF;*.DSF;*.ISO;MASTER1.TOC");
