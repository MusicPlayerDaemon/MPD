/*
* MPD DVD-Audio Decoder plugin
* Copyright (c) 2014 Maxim V.Anisiutkin <maxim.anisiutkin@gmail.com>
*
* DVD-Audio Decoder is free software; you can redistribute it and/or
* modify it under the terms of the GNU Lesser General Public
* License as published by the Free Software Foundation; either
* version 2.1 of the License, or (at your option) any later version.
*
* DVD-Audio Decoder is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
* Lesser General Public License for more details.
*
* You should have received a copy of the GNU Lesser General Public
* License along with FFmpeg; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
*/

#include "config.h"

#include <malloc.h>
#include <string>
#include "util/Domain.hxx"
#include "Log.hxx"
#include "dvda_disc.h"

using namespace std;

static constexpr Domain dvdaiso_domain("dvdaiso");

dvda_disc_t::dvda_disc_t() {
	dvda_media = nullptr;
	dvda_filesystem = nullptr;
	audio_stream = nullptr;
	stream_downmix = false;
	sel_track_index = -1;
}

dvda_disc_t::~dvda_disc_t() {
	close();
	if (audio_stream) {
		delete audio_stream;
	}
}

dvda_filesystem_t* dvda_disc_t::get_filesystem() {
	return dvda_filesystem;
}

audio_track_t* dvda_disc_t::get_track(uint32_t track_index) {
	return track_index < get_tracks() ? &track_list[track_index] : nullptr;
}

uint32_t dvda_disc_t::get_tracks() {
	return track_list.size();
}

uint32_t dvda_disc_t::get_channels() {
	audio_stream_info_t& info = track_list[sel_track_index].audio_stream_info;
	return info.group1_channels + info.group2_channels;
}

uint32_t dvda_disc_t::get_loudspeaker_config() {
	return 0;
}

uint32_t dvda_disc_t::get_samplerate() {
	return track_list[sel_track_index].audio_stream_info.group1_samplerate;
}

double dvda_disc_t::get_duration() {
	return track_list[sel_track_index].duration;
}

double dvda_disc_t::get_duration(uint32_t track_index) {
	if (track_index < track_list.size()) {
		return track_list[track_index].duration;
	}
	return 0.0;
}

bool dvda_disc_t::can_downmix() {
	return track_list[sel_track_index].audio_stream_info.can_downmix;
}

void dvda_disc_t::get_info(uint32_t track_index, bool downmix, const struct TagHandler& handler, void* handler_ctx) {
	if (!(track_index < track_list.size())) {
		return;
	}
	audio_stream_info_t& info = track_list[track_index].audio_stream_info;
	//int ts = track_list[track_index].dvda_titleset;
	//int ti = track_list[track_index].dvda_title;
	int tr = track_list[track_index].dvda_track;

	char disc_label[32];
	bool label_ok = dvda_filesystem->get_name(disc_label);
	disc_label[31] = '\0';

	string disc_path = dvda_media->get_name();
	size_t s0 = disc_path.rfind('/');
	size_t s1 = disc_path.rfind('.');
	string disc_name;
	if (s0 != string::npos && s1 != string::npos) {
		disc_name = disc_path.substr(s0 + 1, s1 - s0 - 1);
	}

	string tag_value;
	tag_value  = label_ok ? disc_label : "DVD-Audio";
	tag_handler_invoke_tag(handler, handler_ctx, TAG_DISC, tag_value.c_str());

	tag_value  = !disc_name.empty() ? disc_name : "Album";
	tag_value += " (";
	if (!downmix) {
		tag_value += to_string(info.group1_channels + info.group2_channels);
		tag_value += "CH";
	}
	else {
		tag_value += "DMX";
	}
	tag_value += "-";
	tag_value += info.stream_id == MLP_STREAM_ID ? (info.stream_type == STREAM_TYPE_MLP ? "MLP" : "TrueHD") : "PCM";
	tag_value += ")";
	tag_handler_invoke_tag(handler, handler_ctx, TAG_ALBUM, tag_value.c_str());

	tag_value  = "Artist";
	tag_handler_invoke_tag(handler, handler_ctx, TAG_ARTIST, tag_value.c_str());

	char track_number_string[16];
	//sprintf(track_number_string, "%02d.%02d.%02d", ts, ti, tr);
	sprintf(track_number_string, "%02d", tr);
	tag_value  = track_number_string;
	tag_value += " - ";
	tag_value += "Track " + to_string(tr);
	tag_value += " (";
	if (!(downmix && track_list[track_index].audio_stream_info.can_downmix)) {
		for (int i = 0; i < info.group1_channels; i++) {
			if (i > 0) {
				tag_value += "-";
			}
			tag_value += info.get_channel_name(i);
		}
		tag_value += " ";
		tag_value += to_string(info.group1_bits);
		tag_value += "/";
		tag_value += to_string(info.group1_samplerate);
		if (info.group2_channels > 0) {
			tag_value += " + ";
			for (int i = 0; i < info.group2_channels; i++) {
				if (i > 0) {
					tag_value += "-";
				}
				tag_value += info.get_channel_name(info.group1_channels + i);
			}
			tag_value += " ";
			tag_value += to_string(info.group2_bits);
			tag_value += "/";
			tag_value += to_string(info.group2_samplerate);
		}
	}
	else {
		tag_value += "DMX ";
		tag_value += to_string(info.group1_bits);
		tag_value += "/";
		tag_value += to_string(info.group1_samplerate);
	}
	tag_value += ")";
	tag_handler_invoke_tag(handler, handler_ctx, TAG_TITLE, tag_value.c_str());

	tag_value  = "Composer";
	tag_handler_invoke_tag(handler, handler_ctx, TAG_COMPOSER, tag_value.c_str());

	tag_value  = "Performer";
	tag_handler_invoke_tag(handler, handler_ctx, TAG_PERFORMER, tag_value.c_str());

	tag_value  = "Genre";
	tag_handler_invoke_tag(handler, handler_ctx, TAG_GENRE, tag_value.c_str());
}

bool dvda_disc_t::open(dvda_media_t* _dvda_media) {
	if (!close()) {
		return false;
	}
	dvda_media = _dvda_media;
	dvda_filesystem = new iso_dvda_filesystem_t;
	if (!dvda_filesystem) {
		return false;
	}
	if (!dvda_filesystem->mount(dvda_media)) {
		return false;
	}
	if (!dvda_zone.open(dvda_filesystem)) {
		return false;
	}
	if (!(dvda_zone.titleset_count() > 0)) {
		return false;
	}
	track_list.init(dvda_zone);
	return track_list.size() > 0;
}

bool dvda_disc_t::close() {
	track_list.clear();
	dvda_zone.close();
	if (dvda_filesystem) {
		delete dvda_filesystem;
		dvda_filesystem = nullptr;
	}
	dvda_media = nullptr;
	sel_track_index = -1;
	return true;
}

bool dvda_disc_t::select_track(uint32_t track_index, size_t offset) {
	sel_track_index = track_index;
	sel_track_offset = offset;
	audio_track = track_list[sel_track_index];
	sel_titleset_index = audio_track.dvda_titleset - 1;
	track_stream.init(512 * DVD_BLOCK_SIZE, 4 * DVD_BLOCK_SIZE, 16 * DVD_BLOCK_SIZE);
	ps1_data.resize(16 * DVD_BLOCK_SIZE);
	stream_block_current = audio_track.block_first;
	stream_size = (audio_track.block_last + 1 - audio_track.block_first) * DVD_BLOCK_SIZE;
	stream_ps1_info.header.stream_id = UNK_STREAM_ID;
	stream_duration = audio_track.duration;
	stream_needs_reinit = false;
	major_sync_0 = false;
	return true;
}

bool dvda_disc_t::get_downmix() {
	return stream_downmix;
}

bool dvda_disc_t::set_downmix(bool downmix) {
	if (downmix && !audio_track.audio_stream_info.can_downmix) {
		return false;
	}
	stream_downmix = downmix;
	return true;
}

bool dvda_disc_t::read_frame(uint8_t* frame_data, size_t* frame_size) {
	decode_run_read_stream_start:
	if (track_stream.is_ready_to_write() && !stream_needs_reinit) {
		stream_buffer_read();
	}
	int data_size = *frame_size, bytes_decoded = 0;
	bytes_decoded = (audio_stream ? audio_stream->decode(frame_data, &data_size, track_stream.get_read_ptr(), track_stream.get_read_size()) : 0);
	if (bytes_decoded <= 0) {
		track_stream.move_read_ptr(0);
		if (bytes_decoded == audio_stream_t::RETCODE_EXCEPT) {
			LogFormat(dvdaiso_domain, LogLevel::ERROR, "Exception occured in DVD-Audio Decoder");
			return false;
		}
		bool decoder_needs_reinit = (bytes_decoded == audio_stream_t::RETCODE_REINIT);
		if (decoder_needs_reinit) {
			if (audio_stream) {
				delete audio_stream;
				audio_stream = nullptr;
			}
			LogFormat(dvdaiso_domain, LogLevel::WARNING, "Reinitializing DVD-Audio Decoder: MLP/TrueHD");
			goto decode_run_read_stream_start;
		}
		if (track_stream.get_read_size() == 0) {
			if (stream_needs_reinit) {
				stream_needs_reinit = false;
				if (audio_stream) {
					delete audio_stream;
					audio_stream = nullptr;
				}
				stream_ps1_info.header.stream_id = UNK_STREAM_ID;
				LogFormat(dvdaiso_domain, LogLevel::WARNING, "Reinitializing DVD-Audio Decoder: PCM");
				goto decode_run_read_stream_start;
			}
			else {
				return false;
			}
		}
		if (audio_stream) {
			int major_sync = audio_stream->resync(track_stream.get_read_ptr(), track_stream.get_read_size());
			if (major_sync == 0) {
				if (major_sync_0) {
					if (track_stream.get_read_size() > 4)
						major_sync = audio_stream->resync(track_stream.get_read_ptr() + 1, track_stream.get_read_size() - 1);
				}
				else
					major_sync_0 = true;
			}
			if (major_sync < 0) {
				if (stream_needs_reinit)
					major_sync = track_stream.get_read_size();
				else
					major_sync = track_stream.get_read_size() > 4 ? track_stream.get_read_size() - 4 : 0;
				if (major_sync <= 0)
					return false;
			}
			if (major_sync > 0) {
				track_stream.move_read_ptr(major_sync);
				LogFormat(dvdaiso_domain, LogLevel::ERROR, "DVD-Audio Decoder is out of sync: %d bytes skipped", major_sync);
			}
			goto decode_run_read_stream_start;
		}
		else {
			create_audio_stream(stream_ps1_info, track_stream.get_read_ptr(), track_stream.get_read_size(), stream_downmix);
			if (audio_stream) {
				if (audio_stream->get_downmix()) {
					audio_stream->set_downmix_coef(audio_track.LR_dmx_coef);
				}
				audio_stream->set_check(false);
				track_stream.move_read_ptr(audio_stream->sync_offset);
			}
			else {
				track_stream.move_read_ptr(DVD_BLOCK_SIZE);
				stream_ps1_info.header.stream_id = UNK_STREAM_ID;
				LogFormat(dvdaiso_domain, LogLevel::ERROR, "DVD-Audio Decoder initialization failed");
			}
			goto decode_run_read_stream_start;
		}
		return false;
	}
	major_sync_0 = false;
	track_stream.move_read_ptr(bytes_decoded);
	*frame_size = data_size;
	return true;
}

bool dvda_disc_t::seek(double seconds) {
	track_stream.reinit();
	if (audio_stream) {
		delete audio_stream;
		audio_stream = nullptr;
	}
	uint32_t offset = (uint32_t)((seconds / (audio_track.duration + 1.0)) * (double)(audio_track.block_last + 1 - audio_track.block_first));
	if (offset > audio_track.block_last - audio_track.block_first - 1) {
		offset = audio_track.block_last - audio_track.block_first - 1;
	}
	stream_block_current = audio_track.block_first + offset;
	stream_ps1_info.header.stream_id = UNK_STREAM_ID;
	return true;
}

bool dvda_disc_t::create_audio_stream(sub_header_t& p_ps1_info, uint8_t* p_buf, int p_buf_size, bool p_downmix) {
	if (audio_stream) {
		delete audio_stream;
		audio_stream = nullptr;
	}
	int init_code = -1;
	switch (stream_ps1_info.header.stream_id) {
	case MLP_STREAM_ID:
		audio_stream = new mlp_audio_stream_t;
		if (audio_stream) {
			init_code = audio_stream->init(p_buf, p_buf_size, p_downmix);
		}
		break;
	case PCM_STREAM_ID:
		audio_stream = new pcm_audio_stream_t;
		if (audio_stream) {
			init_code = audio_stream->init((uint8_t*)&stream_ps1_info.extra_header, p_ps1_info.header.extra_header_length, p_downmix);
		}
		break;
	default:
		break;
	}
	if (!audio_stream) {
		return false;
	}
	if (init_code < 0) {
		delete audio_stream;
		audio_stream = nullptr;
		return false;
	}
	stream_samplerate = audio_stream->group1_samplerate;
	stream_bits = audio_stream->group1_bits > 16 ? 32 : 16;
	if (audio_stream->get_downmix()) {
		stream_channels = 2;
		//stream_channel_map = audio_chunk_t::g_guess_channel_config(pcm_out_channels);
	}
	else {
		stream_channels = audio_stream->group1_channels + audio_stream->group2_channels;
		//stream_channel_map = audio_chunk_t::g_channel_config_from_wfx(audio_stream->get_wfx_channels());
	}
	return true;
}

void dvda_disc_t::stream_buffer_read() {
	sub_header_t ps1_info;
	int blocks_to_read, blocks_read, bytes_written = 0;
	blocks_to_read = track_stream.get_write_size() / DVD_BLOCK_SIZE;
	if (stream_block_current <= audio_track.block_last) {
		if (stream_block_current + blocks_to_read > audio_track.block_last + 1) {
			blocks_to_read = audio_track.block_last + 1 - stream_block_current;
		}
		blocks_read = dvda_zone.get_blocks(sel_titleset_index, stream_block_current, blocks_to_read, track_stream.get_write_ptr());
		dvda_block_t::get_ps1(track_stream.get_write_ptr(), blocks_read, ps1_data.data(), &bytes_written, &ps1_info);
		memcpy(track_stream.get_write_ptr(), ps1_data.data(), bytes_written);
		track_stream.move_write_ptr(bytes_written);
		if (stream_ps1_info.header.stream_id == UNK_STREAM_ID) {
			stream_ps1_info = ps1_info;
		}
		if (blocks_read < blocks_to_read) {
			LogFormat(dvdaiso_domain, LogLevel::ERROR, "DVD-Audio Decoder cannot read track data: titleset = %d, block_number = %d, blocks_to_read = %d", sel_titleset_index, stream_block_current + blocks_read, blocks_to_read - blocks_read);
		}
		stream_block_current += blocks_to_read;
		if (stream_block_current > audio_track.block_last) {
			int blocks_after_last = dvda_zone.get_titleset(sel_titleset_index).get_last() - audio_track.block_last;
			int blocks_to_sync = blocks_after_last < 8 ? blocks_after_last : 8;
			if (stream_block_current <= audio_track.block_last + blocks_to_sync) {
				if (stream_block_current + blocks_to_read > audio_track.block_last + 1 + blocks_to_sync) {
					blocks_to_read = audio_track.block_last + 1 + blocks_to_sync - stream_block_current;
				}
				blocks_read = dvda_zone.get_blocks(sel_titleset_index, stream_block_current, blocks_to_read, track_stream.get_write_ptr());
				bytes_written = 0;
				dvda_block_t::get_ps1(track_stream.get_write_ptr(), blocks_read, ps1_data.data(), &bytes_written, nullptr);
				memcpy(track_stream.get_write_ptr(), ps1_data.data(), bytes_written);
				if (audio_stream) {
					int major_sync = audio_stream->resync(track_stream.get_write_ptr(), bytes_written);
					if (major_sync > 0) {
						track_stream.move_write_ptr(major_sync);
					}
				}
				if (blocks_read < blocks_to_read) {
					LogFormat(dvdaiso_domain, LogLevel::ERROR, "DVD-Audio Decoder cannot read track tail: titleset = %d, block_number = %d, blocks_to_read = %d", sel_titleset_index, stream_block_current + blocks_read, blocks_to_read - blocks_read);
				}
				stream_block_current += blocks_to_read;
			}
		}
	}
}
