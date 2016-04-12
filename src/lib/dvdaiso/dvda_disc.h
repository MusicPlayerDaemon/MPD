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
	dvda_media_t*      dvda_media;
	dvda_filesystem_t* dvda_filesystem;
	dvda_zone_t        dvda_zone;
	track_list_t       track_list;

	stream_buffer_t<uint8_t, int> track_stream;
	vector<uint8_t>               ps1_data;
	audio_stream_t*               audio_stream;
	audio_track_t                 audio_track;

	uint64_t      stream_size;
	double        stream_duration;
	sub_header_t  stream_ps1_info;
	uint32_t      stream_block_current;
	bool          stream_downmix;
	bool          stream_needs_reinit;
	bool          major_sync_0;
	unsigned      stream_channel_map;
	int           stream_channels;
	int           stream_bits;
	int           stream_samplerate;

	int           sel_titleset_index;
	int           sel_track_index;
	size_t        sel_track_offset;
	uint32_t      sel_track_length_lsn;
public:
	dvda_disc_t();
	~dvda_disc_t();
	dvda_filesystem_t* get_filesystem();
	audio_track_t* get_track(uint32_t track_index);
	uint32_t get_tracks();
	uint32_t get_channels();
	uint32_t get_loudspeaker_config();
	uint32_t get_samplerate();
	double get_duration();
	double get_duration(uint32_t track_index);
	bool can_downmix();
    void get_info(uint32_t track_index, bool downmix, const struct TagHandler& handler, void* handler_ctx);
	uint32_t get_track_length_lsn();
	bool open(dvda_media_t* dvda_media);
	bool close();
	bool select_track(uint32_t track_index, size_t offset = 0);
	bool get_downmix();
	bool set_downmix(bool downmix);
	bool read_frame(uint8_t* frame_data, size_t* frame_size);
	bool seek(double seconds);
private:
	bool create_audio_stream(sub_header_t& p_ps1_info, uint8_t* p_buf, int p_buf_size, bool p_downmix);
	void stream_buffer_read();
};

#endif
