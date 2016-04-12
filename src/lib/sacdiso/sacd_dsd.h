/*
* MPD SACD Decoder plugin
* Copyright (c) 2011-2014 Maxim V.Anisiutkin <maxim.anisiutkin@gmail.com>
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

#ifndef _SACD_DSD_H_INCLUDED
#define _SACD_DSD_H_INCLUDED

#include <stdint.h>
#include "endianess.h"

#pragma pack(1)

class ID {
public:
	uint8_t ckID[4];
	bool has_id(const char* id) {
		return ckID[0] == id[0] && ckID[1] == id[1] && ckID[2] == id[2] && ckID[3] == id[3];
	}
	void set_id(const char* id) {
		ckID[0] = id[0];
		ckID[1] = id[1];
		ckID[2] = id[2];
		ckID[3] = id[3];
	}
};

class Chunk : public ID {
public:
	uint64_t ckDataSize;
	uint64_t get_size() {
		return hton64(ckDataSize);
	}
	void set_size(uint64_t size) {
		ckDataSize = hton64(size);
	}
};

#pragma pack()

#endif
