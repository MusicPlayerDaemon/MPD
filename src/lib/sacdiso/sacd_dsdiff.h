/*
* MPD SACD Decoder plugin
* Copyright (c) 2011-2016 Maxim V.Anisiutkin <maxim.anisiutkin@gmail.com>
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

#ifndef _SACD_DSDIFF_H_INCLUDED
#define _SACD_DSDIFF_H_INCLUDED

#include <cstdint>
#include <vector>

#include "endianess.h"
#include "scarletbook.h"
#include "sacd_reader.h"
#include "sacd_dsd.h"

using namespace std;

#pragma pack(1)

class FormDSDChunk : public Chunk {
public:
	ID formType;
};

class DSTFrameIndex {
public:
	uint64_t offset;
	uint32_t length;
};

enum MarkType {TrackStart = 0, TrackStop = 1, ProgramStart = 2, Index = 4};

class Marker {
public:
	uint16_t hours;
	uint8_t  minutes;
	uint8_t  seconds;
	uint32_t samples;
	int32_t  offset;
	uint16_t markType;
	uint16_t markChannel;
	uint16_t TrackFlags;
	uint32_t count;
};

#pragma pack()

class track_t {
public:
	double start_time;
	double stop_time1; // not edited master mode
	double stop_time2; // for edited master mode
};

class id3tags_t {
public:
	uint32_t index;
	uint64_t offset;
	uint64_t size;
	vector<uint8_t> data;
};

class sacd_dsdiff_t : public sacd_reader_t {
	sacd_media_t*       sacd_media;
	open_mode_e         mode;
	area_id_e           track_area;
	uint32_t            version;
	uint32_t            samplerate;
	uint16_t            channel_count;
	uint16_t            loudspeaker_config;
	bool                is_emaster;
	bool                is_dst_encoded;
	uint64_t            frm8_size;
	uint64_t            dsti_offset;
	uint64_t            dsti_size;
	uint64_t            data_offset;
	uint64_t            data_size;
	uint16_t            framerate;
	uint32_t            dsd_frame_size;
	uint32_t            frame_count;
	vector<track_t>     track_index;
	uint64_t            id3_offset;
	vector<id3tags_t>   id3tags;
	uint32_t            current_track;
	uint64_t            current_offset;
	uint64_t            current_size;
public:
	sacd_dsdiff_t();
	virtual ~sacd_dsdiff_t();
	bool open(sacd_media_t* sacd_media, open_mode_e mode = MODE_MULTI_TRACK);
	bool close();
	uint32_t get_tracks();
	uint32_t get_tracks(area_id_e area_id = AREA_BOTH);
	uint32_t get_channels();
	uint32_t get_loudspeaker_config();
	uint32_t get_samplerate();
	uint16_t get_framerate();
	uint64_t get_size();
	uint64_t get_offset();
	double get_duration();
	double get_duration(uint32_t track_index);
	void get_info(uint32_t track_index, const struct TagHandler& handler, void* handler_ctx);
	bool is_dst();
	void set_emaster(bool emaster);
	void select_area(area_id_e area_id);
	bool select_track(uint32_t track_index, area_id_e area_id = AREA_BOTH, uint32_t offset = 0);
	bool read_frame(uint8_t* frame_data, size_t* frame_size, frame_type_e* frame_type);
	bool seek(double seconds);
private:
	uint64_t get_dsti_for_frame(uint32_t frame_nr);
	void get_id3tags(uint32_t track_index, const struct TagHandler& handler, void* handler_ctx);
	void index_id3tags();
};

#endif
