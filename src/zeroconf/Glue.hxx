// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_ZEROCONF_GLUE_HXX
#define MPD_ZEROCONF_GLUE_HXX

#include "Helper.hxx"
#include "config.h"

#include <memory>

struct ConfigData;
class EventLoop;
class ZeroconfHelper;

#ifdef HAVE_ZEROCONF

/**
 * Throws on error.
 */
std::unique_ptr<ZeroconfHelper>
ZeroconfInit(const ConfigData &config, EventLoop &loop);

#endif /* ! HAVE_ZEROCONF */

#endif
