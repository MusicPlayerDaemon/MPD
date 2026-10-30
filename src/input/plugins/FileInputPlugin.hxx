// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include "input/Ptr.hxx"
#include "thread/Mutex.hxx"

class Path;
class UniqueFileHandle;

InputStreamPtr
OpenFileInputStream(Path path, Mutex &mutex);

InputStreamPtr
OpenFileInputStream(Path path, UniqueFileHandle &&file, Mutex &mutex);
