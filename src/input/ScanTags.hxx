// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_INPUT_SCAN_TAGS_HXX
#define MPD_INPUT_SCAN_TAGS_HXX

#include <memory>

class RemoteTagScanner;
class RemoteTagHandler;

/**
 * Find an #InputPlugin which supports the given URI and let it create
 * a #RemoteTagScanner.
 *
 * Throws exception on error
 *
 * @return a #RemoteTagScanner or nullptr if the URI is not supported
 * by any (enabled) plugin
 */
std::unique_ptr<RemoteTagScanner>
InputScanTags(const char *uri, RemoteTagHandler &handler);

#endif
