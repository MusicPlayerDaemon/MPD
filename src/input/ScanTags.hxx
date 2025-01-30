// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include <memory>
#include <string_view>

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
InputScanTags(std::string_view uri, RemoteTagHandler &handler);
