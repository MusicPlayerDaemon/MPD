/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
 * http://www.musicpd.org
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

/** \file
 *
 * Support for locking data structures from the database, for safe
 * multi-threading.
 */

#ifndef MPD_DB_LOCK_HXX
#define MPD_DB_LOCK_HXX

#include "check.h"
#include "thread/Mutex.hxx"
#include "Compiler.h"

#include <assert.h>

extern Mutex db_mutex;

#ifndef NDEBUG

#include "thread/Id.hxx"

extern ThreadId db_mutex_holder;

/**
 * Does the current thread hold the database lock?
 */
gcc_pure
static inline bool
holding_db_lock(void)
{
	return db_mutex_holder.IsInside();
}

#endif

/**
 * Obtain the global database lock.  This is needed before
 * dereferencing a #song or #directory.  It is not recursive.
 */
static inline void
db_lock(void)
{
	assert(!holding_db_lock());

	db_mutex.lock();

	assert(db_mutex_holder.IsNull());
#ifndef NDEBUG
	db_mutex_holder = ThreadId::GetCurrent();
#endif
}

/**
 * Release the global database lock.
 */
static inline void
db_unlock(void)
{
	assert(holding_db_lock());
#ifndef NDEBUG
	db_mutex_holder = ThreadId::Null();
#endif

	db_mutex.unlock();
}

class ScopeDatabaseLock {
public:
	ScopeDatabaseLock() {
		db_lock();
	}

	~ScopeDatabaseLock() {
		db_unlock();
	}
};

#endif
