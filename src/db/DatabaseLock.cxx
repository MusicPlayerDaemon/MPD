// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "DatabaseLock.hxx"

Mutex db_mutex;

#ifndef NDEBUG
ThreadId db_mutex_holder;
#endif
