// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_DATABASE_PTR_HXX
#define MPD_DATABASE_PTR_HXX

#include <memory>

class Database;

typedef std::unique_ptr<Database> DatabasePtr;

#endif
