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

#ifndef DVDA_READER_H
#define DVDA_READER_H

#include "tag/TagHandler.hxx"
#include "dvda_media.h"

class dvda_reader_t {
public:
	dvda_reader_t() {}
	virtual ~dvda_reader_t() {}
	virtual bool open(dvda_media_t* dvda_media) = 0;
	virtual bool close() = 0;
	virtual	uint32_t get_tracks() = 0;
	virtual uint32_t get_channels() = 0;
	virtual uint32_t get_loudspeaker_config() = 0;
	virtual uint32_t get_samplerate() = 0;
	virtual double get_duration() = 0;
	virtual double get_duration(uint32_t track_index) = 0;
	virtual bool can_downmix() = 0;
	virtual void get_info(uint32_t track_index, bool downmix, const struct TagHandler& handler, void* handler_ctx) = 0;
	virtual	bool select_track(uint32_t track_index, size_t offset = 0) = 0;
	virtual	bool get_downmix() = 0;
	virtual	bool set_downmix(bool downmix) = 0;
	virtual bool read_frame(uint8_t* frame_data, size_t* frame_size) = 0;
	virtual bool seek(double seconds) = 0;
};

#endif
