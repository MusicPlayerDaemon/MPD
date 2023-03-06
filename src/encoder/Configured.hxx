// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_ENCODER_CONFIGURED_HXX
#define MPD_ENCODER_CONFIGURED_HXX

struct ConfigBlock;
class PreparedEncoder;

/**
 * Create a #PreparedEncoder instance from the settings in the
 * #ConfigBlock.  Its "encoder" setting is used to choose the encoder
 * plugin.
 *
 * Throws an exception on error.
 *
 * @param shout_legacy enable the "shout" plugin legacy configuration?
 * i.e. fall back to setting "encoding" instead of "encoder"
 */
PreparedEncoder *
CreateConfiguredEncoder(const ConfigBlock &block, bool shout_legacy=false);

#endif
