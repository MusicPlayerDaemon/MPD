// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_INPUT_FILE_HXX
#define MPD_INPUT_FILE_HXX

#include "input/Ptr.hxx"
#include "thread/Mutex.hxx"

class Path;

InputStreamPtr
OpenFileInputStream(Path path, Mutex &mutex);

#endif
