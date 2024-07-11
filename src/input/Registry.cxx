// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

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

constinit const InputPlugin *const input_plugins[] = {
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
	for (const auto &plugin : GetEnabledInputPlugins()) {
		if (plugin.scan_tags != nullptr &&
		    plugin.SupportsUri(uri))
			return true;
	}

	return false;
}
