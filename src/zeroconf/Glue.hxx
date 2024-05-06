// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include "Helper.hxx"

#include <memory>

struct ConfigData;
class EventLoop;
class ZeroconfHelper;

/**
 * Throws on error.
 */
std::unique_ptr<ZeroconfHelper>
ZeroconfInit(const ConfigData &config, EventLoop &loop);
