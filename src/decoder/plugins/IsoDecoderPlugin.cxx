/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
 * http://www.musicpd.org
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <string.h>
#include "config.h"
#include "../DecoderAPI.hxx"
#include "tag/TagHandler.hxx"
#include "fs/Path.hxx"
#include "SacdIsoDecoderPlugin.hxx"
#include "DvdaIsoDecoderPlugin.hxx"
#include "IsoDecoderPlugin.hxx"

using namespace std;

enum iso_type_e {ISO_UNKNOWN = 0, ISO_SACD = 1, ISO_DVDA = 2};

static unsigned
get_container_path_length(const char* path) {
	string container_path = path;
	container_path.resize(strrchr(container_path.c_str(), '/') - container_path.c_str());
	return container_path.length();
}

static iso_type_e
get_iso_type(const char* path) {
	iso_type_e iso_type = ISO_UNKNOWN;
	unsigned length = get_container_path_length(path);
	const char* track_name = path + length + 1;
	if (strncmp(track_name, "2C_AUDIO", 8) == 0 || strncmp(track_name, "MC_AUDIO", 8) == 0) {
		iso_type = ISO_SACD;
	}
	else if (strncmp(track_name, "AUDIO_TS", 8) == 0) {
		iso_type = ISO_DVDA;
	}
	return iso_type;
}

static bool
iso_init(const config_param& param) {
	bool init_ok = false;
#ifdef ENABLE_SACDISO
	init_ok |= sacdiso_decoder_plugin.init(param);
#endif
#ifdef ENABLE_DVDAISO
	init_ok |= dvdaiso_decoder_plugin.init(param);
#endif
	return init_ok;
}

static void
iso_finish() {
#ifdef ENABLE_SACDISO
	sacdiso_decoder_plugin.finish();
#endif
#ifdef ENABLE_DVDAISO
	dvdaiso_decoder_plugin.finish();
#endif
}

static char*
iso_container_scan(Path path_fs, const unsigned int tnum) {
	char* container_path = nullptr;
#ifdef ENABLE_SACDISO
	if (!container_path) {
		container_path = sacdiso_decoder_plugin.container_scan(path_fs, tnum);
	}
#endif
#ifdef ENABLE_DVDAISO
	if (!container_path) {
		container_path = dvdaiso_decoder_plugin.container_scan(path_fs, tnum);
	}
#endif
	return container_path;
}

static void
iso_file_decode(Decoder& decoder, Path path_fs) {
	iso_type_e iso_type = get_iso_type(path_fs.c_str());
	switch (iso_type) {
#ifdef ENABLE_SACDISO
	case ISO_SACD:
		sacdiso_decoder_plugin.file_decode(decoder, path_fs);
		break;
#endif
#ifdef ENABLE_DVDAISO
	case ISO_DVDA:
		dvdaiso_decoder_plugin.file_decode(decoder, path_fs);
		break;
#endif
	default:
		break;
	}
}

static bool
iso_scan_file(Path path_fs, const struct tag_handler* handler, void* handler_ctx) {
	iso_type_e iso_type = get_iso_type(path_fs.c_str());
	bool scan_ok;
	switch (iso_type) {
#ifdef ENABLE_SACDISO
	case ISO_SACD:
		scan_ok = sacdiso_decoder_plugin.scan_file(path_fs, handler, handler_ctx);
		break;
#endif
#ifdef ENABLE_DVDAISO
	case ISO_DVDA:
		scan_ok = dvdaiso_decoder_plugin.scan_file(path_fs, handler, handler_ctx);
		break;
#endif
	default:
		scan_ok = false;
		break;
	}
	return scan_ok;
}

static const char* const iso_suffixes[] = {
	"dat",
	"iso",
	nullptr
};

static const char* const iso_mime_types[] = {
	"application/x-dat",
	"application/x-iso",
	nullptr
};

extern const struct DecoderPlugin diso_decoder_plugin;
const struct DecoderPlugin iso_decoder_plugin = {
	"sacddvdaiso",
	iso_init,
	iso_finish,
	nullptr,
	iso_file_decode,
	iso_scan_file,
	nullptr,
	iso_container_scan,
	iso_suffixes,
	iso_mime_types,
};
