// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_INPUT_CURL_HXX
#define MPD_INPUT_CURL_HXX

#include "lib/curl/Headers.hxx"
#include "input/Ptr.hxx"
#include "thread/Mutex.hxx"

extern const struct InputPlugin input_plugin_curl;

/**
 * Open a #CurlInputStream with custom request headers.
 *
 * This stream does not support Icy metadata.
 *
 * Throws on error.
 */
InputStreamPtr
OpenCurlInputStream(const char *uri, const Curl::Headers &headers,
		    Mutex &mutex);

#endif
