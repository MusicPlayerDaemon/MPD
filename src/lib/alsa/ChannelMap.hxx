// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include "pcm/Export.hxx"

#include <alsa/asoundlib.h>

namespace Alsa {

/**
 * Choose and set an ALSA channel map using snd_pcm_set_chmap().
 *
 * Throws on error.  Logs a warning for non-fatal errors.
 */
void
SetupChannelMap(snd_pcm_t *pcm,
		unsigned channels,
		PcmExport::Params &params);

} // namespace Alsa
