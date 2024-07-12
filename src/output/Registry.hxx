// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include "util/DereferenceIterator.hxx"
#include "util/TerminatedArray.hxx"

struct AudioOutputPlugin;

extern const AudioOutputPlugin *const audio_output_plugins[];

static inline auto
GetAllAudioOutputPlugins() noexcept
{
	return DereferenceContainerAdapter{TerminatedArray<const AudioOutputPlugin *const, nullptr>{audio_output_plugins}};
}

[[gnu::pure]]
const AudioOutputPlugin *
GetAudioOutputPluginByName(const char *name) noexcept;
