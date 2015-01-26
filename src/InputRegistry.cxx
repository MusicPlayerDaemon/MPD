/*
 * Copyright (C) 2003-2013 The Music Player Daemon Project
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

#include "config.h"
#include "InputRegistry.hxx"
#include "util/Macros.hxx"
#include "input/FileInputPlugin.hxx"

#ifdef ENABLE_ARCHIVE
#include "input/ArchiveInputPlugin.hxx"
#endif

#ifdef ENABLE_CURL
#include "input/CurlInputPlugin.hxx"
#endif

#ifdef HAVE_FFMPEG
#include "input/FfmpegInputPlugin.hxx"
#endif

#ifdef ENABLE_MMS
#include "input/MmsInputPlugin.hxx"
#endif

#ifdef ENABLE_CDIO_PARANOIA
#include "input/CdioParanoiaInputPlugin.hxx"
#endif

const InputPlugin *const input_plugins[] = {
	&input_plugin_file,
#ifdef ENABLE_ARCHIVE
	&input_plugin_archive,
#endif
#ifdef ENABLE_CURL
	&input_plugin_curl,
#endif
#ifdef HAVE_FFMPEG
	&input_plugin_ffmpeg,
#endif
#ifdef ENABLE_MMS
	&input_plugin_mms,
#endif
#ifdef ENABLE_CDIO_PARANOIA
	&input_plugin_cdio_paranoia,
#endif
	nullptr
};

bool input_plugins_enabled[ARRAY_SIZE(input_plugins) - 1];
