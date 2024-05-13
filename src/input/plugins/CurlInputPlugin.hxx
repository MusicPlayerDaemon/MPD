// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include "lib/curl/Headers.hxx"
#include "input/Ptr.hxx"
#include "thread/Mutex.hxx"

#include <string_view>

extern const struct InputPlugin input_plugin_curl;

/**
 * Open a #CurlInputStream with custom request headers.
 *
 * This stream does not support Icy metadata.
 *
 * Throws on error.
 */
InputStreamPtr
OpenCurlInputStream(std::string_view uri, const Curl::Headers &headers,
		    Mutex &mutex);
