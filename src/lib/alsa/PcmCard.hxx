// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include <string>

typedef struct _snd_config snd_config_t;

namespace Alsa {

/**
 * Determine the sound card used by the specified PCM device by
 * looking up its definition in the ALSA configuration (without
 * opening it) and return the name of its control device
 * (e.g. "hw:0" or "hw:Intel").
 *
 * @param top the ALSA configuration
 * @return the control device name or an empty string if the card
 * could not be determined (e.g. for PipeWire/PulseAudio PCMs)
 */
std::string
FindPcmCard(snd_config_t *top, const char *pcm_name);

/**
 * Like FindPcmCard(snd_config_t *, const char *), but use the global
 * ALSA configuration.
 */
std::string
FindPcmCard(const char *pcm_name);

} // namespace Alsa
