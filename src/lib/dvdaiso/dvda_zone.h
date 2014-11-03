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

#define PTS_TO_SEC(pts) ((double)pts / 90000.0)

enum dvda_object_type_enum {DVDATypeObject, DVDATypeAOB, DVDATypeSectorPointer, DVDATypeTrack, DVDATypeTitle, DVDATypeTitleset, DVDATypeZone};
enum dvda_titleset_type_enum {DVDTitlesetUnknown, DVDTitlesetAudio, DVDTitlesetVideo};

class dvda_sector_pointer_t;
class dvda_track_t;
class dvda_title_t;
class dvda_titleset_t;
class dvda_zone_t;

class dvda_object_t {
protected:
	dvda_object_type_enum obj_type;
public:
	dvda_object_t() {
		obj_type = DVDATypeObject;
	}
	int get_type() {
		return obj_type;
	}
};

class aob_object_t : public dvda_object_t {
public:
	virtual double get_time() = 0;
	virtual uint32_t get_length_pts() = 0;
	virtual uint32_t get_first() = 0;
	virtual uint32_t get_last() = 0;
};

class dvda_sector_pointer_t : public aob_object_t {
	dvda_track_t* dvda_track;
	int index;
	uint32_t first;
	uint32_t last;
public:
	dvda_sector_pointer_t(dvda_track_t* dvda_track, ats_track_sector_t* p_ats_track_sector, int index);
	double get_time();
	uint32_t get_length_pts();
	uint32_t get_index() {
		return index; 
	}
	uint32_t get_first() {
		return first; 
	}
	uint32_t get_last() {
		return last; 
	}
};

class dvda_track_t : public aob_object_t {
	std::vector<dvda_sector_pointer_t*> dvda_sector_pointers;
	int index;
	int track;
	uint32_t first_pts;
	uint32_t length_pts;
	int downmix_matrix;
public:
	dvda_track_t(ats_track_timestamp_t* p_ats_track_timestamp, int track);
	virtual ~dvda_track_t();
	int get_sector_pointers() {
		return dvda_sector_pointers.size();
	}
	dvda_sector_pointer_t* get_sector_pointer(int i_index) {
		return dvda_sector_pointers[i_index];
	}
	void add_sector_pointer(dvda_sector_pointer_t* dvda_sector_pointer) {
		dvda_sector_pointers.push_back(dvda_sector_pointer);
	}
	uint32_t get_index() {
		return index; 
	}
	int get_track() {
		return track; 
	}
	double get_time() {
		return PTS_TO_SEC(length_pts);
	}
	uint32_t get_length_pts() {
		return length_pts;
	}
	int get_downmix_matrix() {
		return downmix_matrix;
	}
	uint32_t get_first();
	uint32_t get_last();
};

class dvda_title_t : public dvda_object_t {
	std::vector<dvda_track_t*> dvda_tracks;
	int title;
	uint32_t length_pts;
	int indexes;
	int tracks;
public:
	dvda_title_t(ats_title_t* p_ats_title, ats_title_idx_t* p_ats_title_idx);
	virtual ~dvda_title_t();
	int get_tracks() {
		return dvda_tracks.size();
	}
	dvda_track_t* get_track(int i_track) {
		return dvda_tracks[i_track];
	}
	void add_track(dvda_track_t* dvda_track) {
		dvda_tracks.push_back(dvda_track);
	}
	int get_title() {
		return title; 
	}
	double get_time() {
		return PTS_TO_SEC(length_pts);
	}
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
	dvda_zone_t* dvda_zone;
	bool is_open;
	std::vector<dvda_title_t*> dvda_titles;
	dvda_titleset_type_enum dvda_titleset_type;
	dvda_aob_t aobs[9];
	dvda_downmix_matrix_t downmix_matrices[DOWNMIX_MATRICES];
	uint32_t aobs_last_sector;
	int titleset;
public:
	dvda_titleset_t() {
		is_open = false;
	}
	virtual ~dvda_titleset_t();
	int get_titles() {
		return dvda_titles.size();
	}
	dvda_title_t* get_title(int i_title) {
		return dvda_titles[i_title];
	}
	void add_title(dvda_title_t* dvda_title) {
		dvda_titles.push_back(dvda_title);
	}
	uint32_t get_last() {
		return aobs_last_sector; 
	}
	int get_titleset() {
		return titleset;
	}
	bool is_audio_ts() {
		return dvda_titleset_type == DVDTitlesetAudio;
	}
	bool is_video_ts() {
		return dvda_titleset_type == DVDTitlesetVideo;
	}
	double get_downmix_coef(int matrix, int channel, int dmx_channel) {
		if (matrix >= 0 && matrix < DOWNMIX_MATRICES)
			return downmix_matrices[matrix].get_downmix_coef(channel, dmx_channel);
		return 0.0;
	}
	bool open() {
		return is_open;
	}
	bool open(dvda_zone_t* dvda_zone, int titleset);
	DVDAERROR get_block(uint32_t block_no, uint8_t* buf_ptr);
	int get_blocks(uint32_t block_first, uint32_t block_last, uint8_t* buf_ptr);
	void close_aobs();
};

class dvda_zone_t : public dvda_object_t {
	dvda_filesystem_t* dvda_filesystem;
	bool is_open;
	std::vector<dvda_titleset_t*> dvda_titlesets;
	int audio_titlesets;
	int video_titlesets;
public:
	dvda_zone_t() {
		is_open = false;
	}
	~dvda_zone_t() {
	}
	dvda_filesystem_t* get_filesystem() {
		return dvda_filesystem;
	}
	int get_titlesets() {
		int size;
		size = dvda_titlesets.size();
		return size;
	}
	dvda_titleset_t* get_titleset(int titleset) {
		dvda_titleset_t* dvda_titleset;
		dvda_titleset = dvda_titlesets[titleset];
		return dvda_titleset;
	}
	void add_titleset(dvda_titleset_t* dvda_titleset) {
		dvda_titlesets.push_back(dvda_titleset);
	}
	bool open() {
		return is_open;
	}
	bool open(dvda_filesystem_t* dvda_filesystem);
	void close();
	DVDAERROR get_block(int titleset, uint32_t block_no, uint8_t* buf_ptr);
	int get_blocks(int titleset, uint32_t block_no, int blocks, uint8_t* buf_ptr);
};

#endif
