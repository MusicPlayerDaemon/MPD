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

#ifndef _AUDIO_TRACK
#define _AUDIO_TRACK

#include <cstdint>
#include <vector>
#include "dvda_zone.h"

using std::vector;

typedef class {
public:
	int                 dvda_titleset;
	int                 dvda_title;
	int                 dvda_track;
	uint32_t            block_first;
	uint32_t            block_last;
	double              duration;
	double              LR_dmx_coef[DOWNMIX_CHANNELS][2];
	audio_stream_info_t audio_stream_info;
	bool check_chmode(chmode_t chmode, bool downmix);
} audio_track_t;

class track_list_t {
	vector<audio_track_t> track_list;
public:
	size_t size() const {
		return track_list.size();
	}
	void clear() {
		track_list.clear();
	}
	void add(const audio_track_t& audio_track) {
		track_list.push_back(audio_track);
	}
	audio_track_t& operator[](int track_index) {
		return track_list[track_index];
	}
	void init(dvda_zone_t& dvda_zone);
	bool get_audio_stream_info(dvda_zone_t& dvda_zone, int titleset, uint32_t block_no, audio_stream_info_t& audio_stream_info);
};

#endif
