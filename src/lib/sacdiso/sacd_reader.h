/*
* MPD SACD Decoder plugin
* Copyright (c) 2014 Maxim V.Anisiutkin <maxim.anisiutkin@gmail.com>
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

#ifndef _SACD_READER_H_INCLUDED
#define _SACD_READER_H_INCLUDED

#include "tag/TagHandler.hxx"
#include "sacd_media.h"

#define MODE_SINGLE_TRACK  1
#define MODE_FULL_PLAYBACK 2

enum area_id_e {AREA_BOTH = 0, AREA_TWOCH = 1, AREA_MULCH = 2};
enum frame_type_e {FRAME_DSD = 0, FRAME_DST = 1, FRAME_INVALID = -1};

class sacd_reader_t {
public:
	sacd_reader_t() {}
	virtual ~sacd_reader_t() {}
	virtual bool open(sacd_media_t* sacd_media) = 0;
	virtual bool close() = 0;
	virtual	uint32_t get_tracks() = 0;
	virtual	uint32_t get_tracks(area_id_e area_id = AREA_BOTH) = 0;
	virtual uint32_t get_channels() = 0;
	virtual uint32_t get_loudspeaker_config() = 0;
	virtual uint32_t get_samplerate() = 0;
	virtual uint16_t get_framerate() = 0;
	virtual uint64_t get_size() = 0;
	virtual uint64_t get_offset() = 0;
	virtual double get_duration() = 0;
	virtual double get_duration(uint32_t track_index) = 0;
	virtual void get_info(uint32_t track_index, const struct tag_handler* handler, void* handler_ctx) = 0;
	virtual bool is_dst() = 0;
	virtual void set_emaster(bool emaster) = 0;
	virtual void select_area(area_id_e area_id) = 0;
	virtual	bool select_track(uint32_t track_index, area_id_e area_id = AREA_BOTH, uint32_t offset = 0) = 0;
	virtual bool read_frame(uint8_t* frame_data, size_t* frame_size, frame_type_e* frame_type) = 0;
	virtual bool seek(double seconds) = 0;
};

#endif
