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

#ifndef _DVDA_FILESYSTEM_H_INCLUDED
#define _DVDA_FILESYSTEM_H_INCLUDED

#include <cstdint>
#include <string>
#include "dvda_media.h"
#include "dvd_input.h"
#include "dvd_udf.h"

class dvda_filesystem_t;
class dvda_fileobject_t;

class dvda_filesystem_t {
public:
	dvda_filesystem_t() {
	}
	virtual ~dvda_filesystem_t() {
	}
	virtual bool mount(dvda_media_t* dvda_media) = 0;
	virtual void dismount() = 0;
	virtual bool get_name(char* name) = 0;
	virtual dvda_fileobject_t* file_open(const char* name) = 0;
	virtual void file_close(dvda_fileobject_t* fileobject) = 0;
};

class dvda_fileobject_t {
protected:
	int64_t size;
public:
	dvda_fileobject_t() {
		size = 0;
	}
	virtual ~dvda_fileobject_t() {
	}
	virtual bool open(const char* path) = 0;
	virtual bool close() = 0;
	virtual int read(void* buffer, int count) = 0;
	virtual bool seek(int64_t offset) = 0;
	virtual int64_t get_size() {
		return size;
	}
};

class iso_dvda_filesystem_t : public dvda_filesystem_t {
	friend class iso_dvda_fileobject_t;
	dvda_media_t* dvda_media;
	dvd_reader_t* iso_reader;
public:
	iso_dvda_filesystem_t() : dvda_filesystem_t() {
		iso_reader = nullptr;
	}
	bool mount(dvda_media_t* dvda_media);
	void dismount();
	bool get_name(char* name);
	dvda_fileobject_t* file_open(const char* name);
	void file_close(dvda_fileobject_t* fileobject);
};

class iso_dvda_fileobject_t : public dvda_fileobject_t {
	friend class iso_dvda_filesystem_t;
	iso_dvda_filesystem_t* fs;
	dvda_media_t* fo;
	uint32_t lba;
public:
	iso_dvda_fileobject_t() : dvda_fileobject_t() {
		fs = nullptr;
		fo = nullptr;
		lba = 0;
	}
	~iso_dvda_fileobject_t() {
	}
	bool open(const char* path);
	bool close();
	int read(void* buffer, int count);
	bool seek(int64_t offset);
};

#endif
