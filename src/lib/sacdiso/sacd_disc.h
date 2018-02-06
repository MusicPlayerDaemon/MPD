/*
* MPD SACD Decoder plugin
* Copyright (c) 2017 Maxim V.Anisiutkin <maxim.anisiutkin@gmail.com>
*
* This module partially uses code from SACD Ripper http://code.google.com/p/sacd-ripper/ project
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

#ifndef _SACD_DISC_H_INCLUDED
#define _SACD_DISC_H_INCLUDED

#include "config.h"

#include <cstdint>

#include "endianess.h"
#include "scarletbook.h"
#include "sacd_reader.h"

#define CP_ACP 0

#define SACD_PSN_SIZE 2064
#define MAX_DATA_SIZE (1024 * 64)

typedef struct {
	uint8_t data[MAX_DATA_SIZE];
	int     size;
	bool    started;
	int     sector_count;
	int     dst_encoded;
} audio_frame_t;

class sacd_disc_t : public sacd_reader_t {
private:
	sacd_media_t*        sacd_media;
	open_mode_e          mode;
	scarletbook_handle_t sb_handle;
	area_id_e            track_area;
	uint32_t             sel_track_index;
	uint32_t             sel_track_start_lsn;
	uint32_t             sel_track_length_lsn;
	uint32_t             sel_track_current_lsn;
	uint32_t             channel_count;
	bool                 is_emaster;
	bool                 is_dst_encoded;
	audio_sector_t       audio_sector;
	audio_frame_t        frame;
	int                  frame_info_counter;
	int                  packet_info_idx;
	uint8_t              sector_buffer[SACD_PSN_SIZE];
	uint32_t             sector_size;
	int                  sector_bad_reads;
	uint8_t*             buffer;
	int                  buffer_offset;
public:
	sacd_disc_t();
	~sacd_disc_t();
	scarletbook_area_t* get_area(area_id_e area_id);
	uint32_t get_tracks();
	uint32_t get_tracks(area_id_e area_id = AREA_BOTH);
	area_id_e get_track_area_id();
	uint32_t get_track_index();
	uint32_t get_channels();
	uint32_t get_loudspeaker_config();
	uint32_t get_samplerate();
	uint16_t get_framerate();
	uint64_t get_size();
	uint64_t get_offset();
	double get_duration();
	double get_duration(uint32_t track_index);
	void get_info(uint32_t track_index, const struct TagHandler& handler, void* handler_ctx);
	uint32_t get_track_length_lsn();
	bool is_dst();
	void set_emaster(bool emaster);
	bool open(sacd_media_t* sacd_media, open_mode_e mode = MODE_MULTI_TRACK);
	bool close();
	void select_area(area_id_e area_id);
	bool select_track(uint32_t track_index, area_id_e area_id = AREA_BOTH, uint32_t offset = 0);
	bool read_frame(uint8_t* frame_data, size_t* frame_size, frame_type_e* frame_type);
	bool seek(double seconds);
	bool read_blocks_raw(uint32_t lb_start, uint32_t block_count, uint8_t* data);
private:
	scarletbook_handle_t* get_handle();
	bool read_master_toc();
	bool read_area_toc(int area_idx);
	void free_area(scarletbook_area_t* area);
};

#endif
