/*
* DVD-Audio Decoder plugin
* Copyright (c) 2009 Maxim V.Anisiutkin <maxim.anisiutkin@gmail.com>
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

#ifndef _DVDA_BLOCK
#define _DVDA_BLOCK

#include <stdint.h>
#include "audio_stream_info.h"
#include "dvda_error.h"

#define DVD_BLOCK_SIZE 2048
#define SEGMENT_HEADER_BLOCKS 16
#define SEGMENT_HEADER_SIZE (SEGMENT_HEADER_BLOCKS * DVD_BLOCK_SIZE)

typedef struct _sub_header_t {
	struct {
		uint8_t stream_id;
		uint8_t cyclic;
		uint8_t padding1;
		uint8_t extra_header_length;
	} header;
	union {
		struct {
			uint16_t first_audio_frame;
			uint8_t padding1;
			uint8_t group2_bits : 4;
			uint8_t group1_bits : 4;
			uint8_t group2_samplerate : 4;
			uint8_t group1_samplerate : 4;
			uint8_t padding2;
			uint8_t channel_assignment;
			uint8_t padding3;
			uint8_t cci;
		} pcm;
		struct {
			uint8_t padding1;
			uint8_t padding2;
			uint8_t padding3;
			uint8_t padding4;
			uint8_t cci;
		} mlp;
	} extra_header;
	uint8_t padding[256];
} sub_header_t;

class dvda_block_t {
	static int get_ps1_info_length(uint8_t* p_substream_buffer, int substream_length);
public:
	static void get_ps1(uint8_t* p_block, uint8_t* p_ps1_buffer, int* p_ps1_offset, sub_header_t* p_ps1_info);
	static void get_ps1(uint8_t* p_block, int blocks, uint8_t* p_ps1_buffer, int* p_ps1_offset, sub_header_t* p_ps1_info);
};

#endif
