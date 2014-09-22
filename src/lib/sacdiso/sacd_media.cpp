/*
* SACD Decoder plugin
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

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "sacd_media.h"

sacd_media_file_t::sacd_media_file_t() {
	fd = -1;
}

sacd_media_file_t::~sacd_media_file_t() {
	close();
}

bool sacd_media_file_t::open(const char* path) {
	int ret;
	struct stat64 st;
	fd = ::open(path, O_RDONLY, 0);
	if (fd < 0) {
		return false;
	}
	ret = ::fstat64(fd, &st);
	if (ret < 0) {
		::close(fd);
		fd = -1;
		return false;
	}
	if (!S_ISREG(st.st_mode)) {
		::close(fd);
		fd = -1;
		return false;
	}
	return true;
}

bool sacd_media_file_t::close() {
	if (fd < 0) {
		return false;
	}
	::close(fd);
	fd = -1;
	return true;
}

bool sacd_media_file_t::seek(int64_t position, int mode) {
	off64_t ret = ::lseek64(fd, (off64_t)position, mode);
	if (ret < 0) {
		return false;
	}
	return true;
}

int64_t sacd_media_file_t::get_position() {
	return ::lseek64(fd, 0, SEEK_CUR);
}

int64_t sacd_media_file_t::get_size() {
	int ret;
	struct stat64 st;
	ret = ::fstat64(fd, &st);
	if (ret < 0) {
		return 0;
	}
	return st.st_size;
}

size_t sacd_media_file_t::read(void* data, size_t size) {
	return ::read(fd, data, size);
}

int64_t sacd_media_file_t::skip(int64_t bytes) {
	return ::lseek64(fd, (off64_t)bytes, SEEK_CUR);
}
