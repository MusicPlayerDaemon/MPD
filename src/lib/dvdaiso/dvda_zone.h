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

#ifndef _DVDA_ZONE_H_INCLUDED
#define _DVDA_ZONE_H_INCLUDED

#include <cstdint>
#include <vector>
#include "audio_stream.h"
#include "ifo.h"
#include "dvda_block.h"
#include "dvda_error.h"
#include "dvda_filesystem.h"

using namespace std;

#define PTS_TO_SEC(pts) ((double)pts / 90000.0)

enum dvd_type_e {DVDTypeObject, DVDTypeAOB, DVDTypeSectorPointer, DVDTypeTrack, DVDTypeTitle, DVDTypeTitleset, DVDTypeZone};
enum dvd_titleset_e {DVDTitlesetUnknown, DVDTitlesetAudio, DVDTitlesetVideo};

class dvda_sector_pointer_t;
class dvda_track_t;
class dvda_title_t;
class dvda_titleset_t;
class dvda_zone_t;

class dvda_object_t {
	dvd_type_e obj_type;
public:
	dvda_object_t(dvd_type_e type = DVDTypeObject) {
		obj_type = type;
	}
	int get_type() {
		return obj_type;
	}
	void set_type(dvd_type_e type) {
		obj_type = type;
	}
};

class aob_object_t : public dvda_object_t {
public:
	aob_object_t(dvd_type_e type = DVDTypeAOB);
	~aob_object_t();
	virtual double get_time();
	virtual uint32_t get_length_pts() = 0;
	virtual uint32_t get_first() = 0;
	virtual uint32_t get_last() = 0;
};

class dvda_sector_pointer_t : public aob_object_t {
	dvda_track_t* track;
	int index;
	uint32_t first;
	uint32_t last;
public:
	dvda_sector_pointer_t(dvda_track_t* track, ats_track_sector_t* p_ats_track_sector, int index);
	~dvda_sector_pointer_t();
	uint32_t get_index();
	uint32_t get_length_pts();
	uint32_t get_first();
	uint32_t get_last();
};

class dvda_track_t : public aob_object_t {
	vector<dvda_sector_pointer_t> sector_pointers;
	int index;
	int track;
	uint32_t first_pts;
	uint32_t length_pts;
	int downmix_matrix;
public:
	dvda_track_t(ats_track_timestamp_t* p_ats_track_timestamp, int track);
	~dvda_track_t();
	int sector_pointer_count();
	dvda_sector_pointer_t& get_sector_pointer(int sector_pointer_index);
	void append(dvda_sector_pointer_t& dvda_sector_pointer);
	uint32_t get_index();
	int get_track();
	int get_downmix_matrix();
	uint32_t get_length_pts();
	uint32_t get_first();
	uint32_t get_last();
};

class dvda_title_t : public dvda_object_t {
	vector<dvda_track_t> tracks;
	uint32_t length_pts;
	int ats_title;
	int ats_indexes;
	int ats_tracks;
public:
	dvda_title_t(ats_title_t* p_ats_title, ats_title_idx_t* p_ats_title_idx);
	~dvda_title_t();
	int track_count();
	dvda_track_t& get_track(int track_index);
	void append(dvda_track_t& track);
	int get_title();
	double get_time();
};

class dvda_aob_t {
public:
	char               file_name[13];
	uint32_t           block_first;
	uint32_t           block_last;
	dvda_fileobject_t* dvda_fileobject;
};

class dvda_downmix_channel_t {
public:
	bool    inv_phase;
	uint8_t coef;
};

class dvda_downmix_matrix_t {
	dvda_downmix_channel_t LR_dmx[DOWNMIX_CHANNELS][2];
public:
	dvda_downmix_channel_t* get_downmix_channel(int channel, int dmx_channel);
	double get_downmix_coef(int channel, int dmx_channel);
};

class dvda_titleset_t : public dvda_object_t {
	dvda_zone_t* zone;
	vector<dvda_title_t> titles;
	dvd_titleset_e titleset_type;
	dvda_aob_t aobs[9];
	uint32_t aobs_last_sector;
	dvda_downmix_matrix_t downmix_matrices[DOWNMIX_MATRICES];
public:
	dvda_titleset_t();
	~dvda_titleset_t();
	int title_count();
	dvda_title_t& get_title(int title_index);
	void append(dvda_title_t& title);
	uint32_t get_last();
	bool is_audio_ts();
	bool is_video_ts();
	double get_downmix_coef(int matrix, int channel, int dmx_channel);
	bool open(dvda_zone_t* zone, int titleset_index);
	void close();
	DVDAERROR get_block(uint32_t block_index, uint8_t* buf_ptr);
	int get_blocks(uint32_t block_first, uint32_t block_last, uint8_t* block_data);
};

class dvda_zone_t : public dvda_object_t {
	dvda_filesystem_t* filesystem;
	vector<dvda_titleset_t> titlesets;
	int audio_titlesets;
	int video_titlesets;
public:
	dvda_zone_t();
	~dvda_zone_t();
	dvda_filesystem_t* get_filesystem();
	int titleset_count();
	dvda_titleset_t& get_titleset(int titleset_index);
	void append(dvda_titleset_t& titleset);
	bool open(dvda_filesystem_t* filesystem);
	void close();
	DVDAERROR get_block(int titleset_index, uint32_t block_index, uint8_t* block_data);
	int get_blocks(int titleset_index, uint32_t block_index, int block_count, uint8_t* block_data);
};

#endif
