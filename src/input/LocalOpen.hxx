// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include "Ptr.hxx"
#include "thread/Mutex.hxx"

class Path;
class UniqueFileHandle;

/**
 * Open a "local" file.  This is a wrapper for the input plugins
 * "file" and "archive".
 *
 * Throws std::runtime_error on error.
 */
InputStreamPtr
OpenLocalInputStream(Path path, Mutex &mutex);

#ifndef _WIN32

InputStreamPtr
OpenLocalInputStream(Path path, UniqueFileHandle &&file, Mutex &mutex);

#endif
