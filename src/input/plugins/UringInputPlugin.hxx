// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include "input/Ptr.hxx"
#include "thread/Mutex.hxx"

class EventLoop;

/**
 * Initialize the plugin.  This needs to be done manually because this
 * plugin is not a regular plugin and doesn't have an #InputPlugin
 * instance, and thus no "init" function.
 */
void
InitUringInputPlugin(EventLoop &event_loop) noexcept;

/**
 * Check whether `io_uring` is available.
 */
bool
IsUringInputPluginAvailable() noexcept;

/**
 * Create an #InputStream that reads from a local file using
 * `io_uring`.
 *
 * Requires an InitUringInputPlugin() call during process
 * initialization.  May only be called if
 * IsUringInputPluginAvailable() has returned true.
 */
InputStreamPtr
OpenUringInputStream(const char *path, Mutex &mutex);
