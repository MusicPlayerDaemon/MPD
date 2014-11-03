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

#include <string>
#include <string.h>
#include "dvda_filesystem.h"

using namespace std;

bool iso_dvda_fileobject_t::open(const char* path) {
	(void)path;
	return true;
}

bool iso_dvda_fileobject_t::close() {
	return true;
}

int iso_dvda_fileobject_t::read(void* buffer, int count) {
	count = fo->read(buffer, count);
	return count;
}

bool iso_dvda_fileobject_t::seek(int64_t offset) {
	bool ok;
	ok = false;
	if (offset < size) {
		ok = fo->seek((int64_t)2048 * (int64_t)lba + offset);
	}
	return ok;
}

bool iso_dvda_filesystem_t::mount(dvda_media_t* _dvda_media) {
	dvda_media = _dvda_media;
	iso_reader = DVDOpen(dvda_media);
	return iso_reader != nullptr;
}

void iso_dvda_filesystem_t::dismount() {
	if (iso_reader) {
		DVDClose(iso_reader);
	}
}

bool iso_dvda_filesystem_t::get_name(char* name) {
	bool ok;
	ok = UDFGetVolumeIdentifier(iso_reader, name, 32) > 0;
	return ok;
}

dvda_fileobject_t* iso_dvda_filesystem_t::file_open(const char* name) {
	iso_dvda_fileobject_t* fileobject = new iso_dvda_fileobject_t;
	if (!fileobject) {
		return nullptr;
	}
	fileobject->fs = this;
	fileobject->fo = dvda_media;
	string filepath = "/AUDIO_TS/";
	filepath += name;
	uint32_t filesize = 0;
	fileobject->lba = UDFFindFile(iso_reader, (char*)filepath.c_str(), &filesize);
	fileobject->size = filesize;
	if (fileobject->lba == 0) {
		delete fileobject;
		return nullptr;
	}
	if (!fileobject->open(filepath.c_str())) {
		delete fileobject;
		return nullptr;
	}
	if (!fileobject->seek(0)) {
		fileobject->close();
		delete fileobject;
		return nullptr;
	}
	return fileobject;
}

void iso_dvda_filesystem_t::file_close(dvda_fileobject_t* fileobject) {
	if (fileobject) {
		fileobject->close();
		delete fileobject;
	}
}
