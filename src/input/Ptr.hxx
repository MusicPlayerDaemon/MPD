// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_INPUT_STREAM_PTR_HXX
#define MPD_INPUT_STREAM_PTR_HXX

#include <memory>

class InputStream;

typedef std::unique_ptr<InputStream> InputStreamPtr;

#endif
