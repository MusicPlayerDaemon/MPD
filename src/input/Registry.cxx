/*
 * Copyright 2003-2021 The Music Player Daemon Project
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

#include "Registry.hxx"
#include "InputPlugin.hxx"
#include "input/Features.h"
#include "plugins/QobuzInputPlugin.hxx"
#include "config.h"

#ifdef ENABLE_ALSA
#include "plugins/AlsaInputPlugin.hxx"
#endif

#ifdef ENABLE_CURL
#include "plugins/CurlInputPlugin.hxx"
#endif

#ifdef ENABLE_FFMPEG
#include "plugins/FfmpegInputPlugin.hxx"
#endif

#ifdef ENABLE_SMBCLIENT
#include "plugins/SmbclientInputPlugin.hxx"
#endif

#ifdef ENABLE_NFS
#include "plugins/NfsInputPlugin.hxx"
#endif

#ifdef ENABLE_MMS
#include "plugins/MmsInputPlugin.hxx"
#endif

#ifdef ENABLE_CDIO_PARANOIA
#include "plugins/CdioParanoiaInputPlugin.hxx"
#endif

constexpr const InputPlugin *input_plugins[] = {
#ifdef ENABLE_ALSA
	&input_plugin_alsa,
#endif
#ifdef ENABLE_QOBUZ
	&qobuz_input_plugin,
#endif
#ifdef ENABLE_CURL
	&input_plugin_curl,
#endif
#ifdef ENABLE_FFMPEG
	&input_plugin_ffmpeg,
#endif
#ifdef ENABLE_SMBCLIENT
	&input_plugin_smbclient,
#endif
#ifdef ENABLE_NFS
	&input_plugin_nfs,
#endif
#ifdef ENABLE_MMS
	&input_plugin_mms,
#endif
#ifdef ENABLE_CDIO_PARANOIA
	&input_plugin_cdio_paranoia,
#endif
	nullptr
};

static constexpr std::size_t n_input_plugins = std::size(input_plugins) - 1;

/* the std::max() is just here to avoid a zero-sized array, which is
   forbidden in C++ */
bool input_plugins_enabled[std::max(n_input_plugins, std::size_t(1))];

bool
HasRemoteTagScanner(const char *uri) noexcept
{
	input_plugins_for_each_enabled(plugin)
		if (plugin->scan_tags != nullptr &&
		    plugin->SupportsUri(uri))
			return true;

	return false;
}
