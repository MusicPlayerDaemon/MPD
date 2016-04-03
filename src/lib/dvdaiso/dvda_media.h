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

#ifndef _DVDA_MEDIA_H_INCLUDED
#define _DVDA_MEDIA_H_INCLUDED

#include "config.h"
#include <stdint.h>
#include <stddef.h>
#include <string>
#include "thread/Mutex.hxx"
#include "thread/Cond.hxx"
#include "input/InputStream.hxx"
#include "util/Error.hxx"
#include "Log.hxx"

using namespace std;

enum media_type_t {UNK_TYPE = 0, IFO_TYPE = 1, ISO_TYPE = 2, MLP_TYPE = 3, AOB_TYPE = 4};

class dvda_media_t {
public:
	dvda_media_t() {}
	virtual ~dvda_media_t() {}
	virtual const char* get_name() = 0;
	virtual int64_t get_position() = 0;
	virtual int64_t get_size() = 0;
	virtual bool    open(const char* path) = 0;
	virtual bool    close() = 0;
	virtual bool    seek(int64_t position) = 0;
	virtual size_t  read(void* data, size_t size) = 0;
	virtual int64_t skip(int64_t bytes) = 0;
};

class dvda_media_file_t : public dvda_media_t {
	string fname;
	int fd;
public:
	dvda_media_file_t();
	~dvda_media_file_t();
	const char* get_name();
	int64_t get_position();
	int64_t get_size();
	bool    open(const char* path);
	bool    close();
	bool    seek(int64_t position);
	size_t  read(void* data, size_t size);
	int64_t skip(int64_t bytes);
};

class dvda_media_stream_t : public dvda_media_t {
	Mutex mutex;
	Cond cond;
    InputStreamPtr is;
public:
	dvda_media_stream_t();
	~dvda_media_stream_t();
	const char* get_name();
	int64_t get_position();
	int64_t get_size();
	bool    open(const char* path);
	bool    close();
	bool    seek(int64_t position);
	size_t  read(void* data, size_t size);
	int64_t skip(int64_t bytes);
};

#endif
