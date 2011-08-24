/*
 * Copyright (C) 2003-2011 The Music Player Daemon Project
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
#include "input_registry.h"
#include "input/file_input_plugin.h"

#ifdef ENABLE_ARCHIVE
#include "input/archive_input_plugin.h"
#endif

#ifdef ENABLE_CURL
#include "input/curl_input_plugin.h"
#endif

#ifdef ENABLE_SOUP
#include "input/soup_input_plugin.h"
#endif

#ifdef HAVE_FFMPEG
#include "input/ffmpeg_input_plugin.h"
#endif

#ifdef ENABLE_MMS
#include "input/mms_input_plugin.h"
#endif

#ifdef ENABLE_CDIO_PARANOIA
#include "input/cdio_paranoia_input_plugin.h"
#endif

#ifdef ENABLE_DESPOTIFY
#include "input/despotify_input_plugin.h"
#endif

#include <glib.h>

const struct input_plugin *const input_plugins[] = {
	&input_plugin_file,
#ifdef ENABLE_ARCHIVE
	&input_plugin_archive,
#endif
#ifdef ENABLE_CURL
	&input_plugin_curl,
#endif
#ifdef ENABLE_SOUP
	&input_plugin_soup,
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
#ifdef ENABLE_DESPOTIFY
	&input_plugin_despotify,
#endif
	NULL
};

bool input_plugins_enabled[G_N_ELEMENTS(input_plugins) - 1];
