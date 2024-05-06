// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "config.h"
#include "Registry.hxx"
#include "OutputPlugin.hxx"
#include "output/Features.h"
#include "plugins/AlsaOutputPlugin.hxx"
#include "plugins/AoOutputPlugin.hxx"
#include "plugins/FifoOutputPlugin.hxx"
#include "plugins/SndioOutputPlugin.hxx"
#include "plugins/snapcast/SnapcastOutputPlugin.hxx"
#include "plugins/httpd/HttpdOutputPlugin.hxx"
#include "plugins/JackOutputPlugin.hxx"
#include "plugins/NullOutputPlugin.hxx"
#include "plugins/OpenALOutputPlugin.hxx"
#include "plugins/OssOutputPlugin.hxx"
#include "plugins/PipeOutputPlugin.hxx"
#include "plugins/PipeWireOutputPlugin.hxx"
#include "plugins/PulseOutputPlugin.hxx"
#include "plugins/RecorderOutputPlugin.hxx"
#include "plugins/ShoutOutputPlugin.hxx"
#include "plugins/sles/SlesOutputPlugin.hxx"
#include "plugins/SolarisOutputPlugin.hxx"
#ifdef ENABLE_WINMM_OUTPUT
#include "plugins/WinmmOutputPlugin.hxx"
#endif
#ifdef ENABLE_WASAPI_OUTPUT
#include "plugins/wasapi/WasapiOutputPlugin.hxx"
#endif
#include "util/StringAPI.hxx"

constinit const AudioOutputPlugin *const audio_output_plugins[] = {
#ifdef HAVE_SHOUT
	&shout_output_plugin,
#endif
	&null_output_plugin,
#ifdef ANDROID
	&sles_output_plugin,
#endif
#ifdef HAVE_FIFO
	&fifo_output_plugin,
#endif
#ifdef ENABLE_SNDIO
	&sndio_output_plugin,
#endif
#ifdef ENABLE_PIPE_OUTPUT
	&pipe_output_plugin,
#endif
#ifdef ENABLE_ALSA
	&alsa_output_plugin,
#endif
#ifdef ENABLE_AO
	&ao_output_plugin,
#endif
#ifdef HAVE_OSS
	&oss_output_plugin,
#endif
#ifdef HAVE_OPENAL
	&openal_output_plugin,
#endif
#ifdef ENABLE_SOLARIS_OUTPUT
	&solaris_output_plugin,
#endif
#ifdef ENABLE_PIPEWIRE
	&pipewire_output_plugin,
#endif
#ifdef ENABLE_PULSE
	&pulse_output_plugin,
#endif
#ifdef ENABLE_JACK
	&jack_output_plugin,
#endif
#ifdef ENABLE_HTTPD_OUTPUT
	&httpd_output_plugin,
#endif
#ifdef ENABLE_SNAPCAST_OUTPUT
	&snapcast_output_plugin,
#endif
#ifdef ENABLE_RECORDER_OUTPUT
	&recorder_output_plugin,
#endif
#ifdef ENABLE_WINMM_OUTPUT
	&winmm_output_plugin,
#endif
#ifdef ENABLE_WASAPI_OUTPUT
	&wasapi_output_plugin,
#endif
	nullptr
};

const AudioOutputPlugin *
GetAudioOutputPluginByName(const char *name) noexcept
{
	audio_output_plugins_for_each(plugin)
		if (StringIsEqual(plugin->name, name))
			return plugin;

	return nullptr;
}
