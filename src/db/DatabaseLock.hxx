// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

/** \file
 *
 * Support for locking data structures from the database, for safe
 * multi-threading.
 */

#ifndef MPD_DB_LOCK_HXX
#define MPD_DB_LOCK_HXX

#include "thread/Mutex.hxx"

#include <cassert>

extern Mutex db_mutex;

#ifndef NDEBUG

#include "thread/Id.hxx"

extern ThreadId db_mutex_holder;

/**
 * Does the current thread hold the database lock?
 */
[[gnu::pure]]
static inline bool
holding_db_lock() noexcept
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
	bool locked = true;

public:
	ScopeDatabaseLock() {
		db_lock();
	}

	~ScopeDatabaseLock() {
		if (locked)
			db_unlock();
	}

	/**
	 * Unlock the mutex now, making the destructor a no-op.
	 */
	void unlock() {
		assert(locked);

		db_unlock();
		locked = false;
	}
};

/**
 * Unlock the database while in the current scope.
 */
class ScopeDatabaseUnlock {
public:
	ScopeDatabaseUnlock() {
		db_unlock();
	}

	~ScopeDatabaseUnlock() {
		db_lock();
	}
};

#endif
