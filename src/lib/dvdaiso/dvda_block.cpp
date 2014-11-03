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

#include <memory.h>
#include "dvda_block.h"

int dvda_block_t::get_ps1_info_length(uint8_t* p_substream_buffer, int substream_length) {
	int header_length = 0;
	sub_header_t* sub_header = (sub_header_t*)p_substream_buffer;
	if (substream_length > 4) {
		switch (sub_header->header.stream_id) {
		case PCM_STREAM_ID:
		case MLP_STREAM_ID:
			header_length = sizeof(sub_header->header) + sub_header->header.extra_header_length;
			break;
		default:
			break;
		}
	}
	return header_length;
}

void dvda_block_t::get_ps1(uint8_t* p_block, uint8_t* p_ps1_buffer, int* p_ps1_offset, sub_header_t* p_ps1_info) {
	uint8_t* p_ps1_header;
	uint8_t* p_ps1_body;
	uint8_t* p_ps1_end;
	uint8_t* p_curr = p_block;
	int pes_length;
	int ps1_offset = 0;
	if (*(uint32_t*)p_curr == 0xba010000) {
		p_curr += 14 + (p_curr[13] & 0x07);
		while (p_curr < p_block + DVD_BLOCK_SIZE - 6) {
			pes_length = (p_curr[4] << 8) + p_curr[5];
			if ((*(uint32_t*)p_curr & 0x00ffffff) == 0x00010000) {
				if (p_curr[3] == 0xbd) { // check for private stream 1
					if (p_curr < p_block + DVD_BLOCK_SIZE - 9) {
						p_ps1_header = p_curr + 9 + p_curr[8];
						p_ps1_end = p_curr + 6 + pes_length;
						if (p_ps1_header < p_ps1_end && p_ps1_end <= p_block + DVD_BLOCK_SIZE) {
							int ps1_header_length = get_ps1_info_length(p_ps1_header, p_ps1_end - p_ps1_header);
							if (p_ps1_info && p_ps1_info->header.stream_id == UNK_STREAM_ID && ps1_header_length > 0)
								memcpy(p_ps1_info, p_ps1_header, ps1_header_length < sizeof(sub_header_t) ? ps1_header_length : sizeof(sub_header_t));
							p_ps1_body = p_ps1_header + ps1_header_length;
							int ps1_body_length = p_ps1_end - p_ps1_body;
							if (ps1_body_length > 0) {
								memcpy(p_ps1_buffer + *p_ps1_offset + ps1_offset, p_ps1_body, ps1_body_length);
								ps1_offset += ps1_body_length;
							}
						}
					}
				}
				p_curr += 6 + pes_length;
			}
			else
				break;
		}
	}
	*p_ps1_offset += ps1_offset;
}

void dvda_block_t::get_ps1(uint8_t* p_block, int blocks, uint8_t* p_ps1_buffer, int* p_ps1_offset, sub_header_t* p_ps1_info) {
	if (p_ps1_info)
		p_ps1_info->header.stream_id = UNK_STREAM_ID;
	for (int i = 0; i < blocks; i++)
		get_ps1(p_block + i * DVD_BLOCK_SIZE, p_ps1_buffer, p_ps1_offset, p_ps1_info);
}
