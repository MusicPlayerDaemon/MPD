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

#ifndef DVDA_DISC_H_INCLUDED
#define DVDA_DISC_H_INCLUDED

#include "audio_stream.h"
#include "audio_track.h"
#include "stream_buffer.h"
#include "dvda_reader.h"
#include "dvda_filesystem.h"
#include "dvda_zone.h"


class dvda_disc_t : public dvda_reader_t {
private:
	FILE*                f_ps1;
	dvda_media_t*        dvda_media;
	dvda_filesystem_t*   dvda_filesystem;
	dvda_zone_t*         dvda_zone;
	track_list_t         track_list;
	bool                 no_downmixes;
	bool                 no_short_tracks;

	stream_buffer_t<uint8_t, int> track_stream;
	uint64_t                      track_stream_bytes_aob;
	uint64_t                      track_stream_bytes_ps1;
	audio_stream_t*               audio_stream;
	audio_track_t*                audio_track;
	uint64_t                      stream_size;
	double                        stream_duration;
	sub_header_t                  stream_ps1_info;
	uint32_t                      stream_block_current;
	bool                          stream_needs_reinit;
	bool                          major_sync_0;
	vector<uint8_t>               pcm_out_buffer;
	size_t                        pcm_out_buffer_size;
	unsigned                      pcm_out_channel_map;
	unsigned                 stream_channel_map;
	int                      stream_channels;
	int                      stream_bits;
	int                      stream_samplerate;




	int                  sel_titleset_index;
	audio_track_t        sel_audio_track;
	int                  sel_track_index;
	size_t               sel_track_offset;

	uint32_t             sel_track_start_lsn;
	uint32_t             sel_track_length_lsn;
	uint32_t             sel_track_current_lsn;
	uint32_t             channel_count;
	bool                 is_emaster;
	bool                 is_dst_encoded;
	//audio_sector_t       audio_sector;
	//audio_frame_t        frame;
	int                  frame_info_counter;
	int                  packet_info_idx;
	uint8_t              sector_buffer[2048];
	uint32_t             sector_size;
	int                  sector_bad_reads;
	uint8_t*             buffer;
	int                  buffer_offset;
public:
	dvda_disc_t(bool no_downmixes = false, bool no_short_tracks = false);
	~dvda_disc_t();
	uint32_t get_tracks();
	uint32_t get_track_index();
	uint32_t get_channels();
	uint32_t get_loudspeaker_config();
	uint32_t get_samplerate();
	uint64_t get_size();
	uint64_t get_offset();
	double get_duration();
	double get_duration(uint32_t track_index);
	void get_info(uint32_t track_index, const struct tag_handler *handler, void *handler_ctx);
	uint32_t get_track_length_lsn();
	bool open(dvda_media_t* dvda_media);
	bool close();
	bool select_track(uint32_t track_index, size_t offset = 0);
	bool read_frame(uint8_t* frame_data, size_t* frame_size);
	bool seek(double seconds);
	bool read_blocks_raw(uint32_t lb_start, uint32_t block_count, uint8_t* data);
private:
	 bool create_audio_stream(sub_header_t& p_ps1_info, uint8_t* p_buf, int p_buf_size, bool p_downmix);
	 void stream_buffer_read();
};

#endif
