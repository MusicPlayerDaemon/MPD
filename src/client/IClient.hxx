// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include "config.h"

class Path;
class Database;
class Storage;
struct Partition;

/**
 * An abstract interface for #Client which can be used for unit tests
 * instead of the full #Client class.
 */
class IClient {
public:
	/**
	 * Is this client allowed to use the specified local file?
	 *
	 * Note that this function is vulnerable to timing/symlink attacks.
	 * We cannot fix this as long as there are plugins that open a file by
	 * its name, and not by file descriptor / callbacks.
	 *
	 * Throws #std::runtime_error on error.
	 *
	 * @param path_fs the absolute path name in filesystem encoding
	 */
	virtual void AllowFile(Path path_fs) const = 0;

#ifdef ENABLE_DATABASE
	[[gnu::pure]]
	virtual const Database *GetDatabase() const noexcept = 0;

	[[gnu::pure]]
	virtual const Storage *GetStorage() const noexcept = 0;
#endif // ENABLE_DATABASE
};
