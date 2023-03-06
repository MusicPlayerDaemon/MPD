// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_APE_LOADER_HXX
#define MPD_APE_LOADER_HXX

#include <functional>
#include <string_view>

class InputStream;

typedef std::function<bool(unsigned long flags, const char *key,
			   std::string_view value)> ApeTagCallback;

/**
 * Scans the APE tag values from a file.
 *
 * Throws on I/O error.
 *
 * @return false if the file could not be opened or if no APE tag is
 * present
 */
bool
tag_ape_scan(InputStream &is, const ApeTagCallback& callback);

#endif
