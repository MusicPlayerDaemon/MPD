// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

/** \file
 *
 * Not really a decoder; this plugin forwards its input data "as-is".
 *
 * It was written only to support the "cdio_paranoia" input plugin,
 * which does not need a decoder.
 */

#ifndef MPD_DECODER_PCM_HXX
#define MPD_DECODER_PCM_HXX

extern const struct DecoderPlugin pcm_decoder_plugin;

#endif
