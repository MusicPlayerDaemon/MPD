// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_INPUT_LOCAL_OPEN_HXX
#define MPD_INPUT_LOCAL_OPEN_HXX

#include "Ptr.hxx"
#include "thread/Mutex.hxx"

class Path;

/**
 * Open a "local" file.  This is a wrapper for the input plugins
 * "file" and "archive".
 *
 * Throws std::runtime_error on error.
 */
InputStreamPtr
OpenLocalInputStream(Path path, Mutex &mutex);

#endif
