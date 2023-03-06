// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_NFS_CALLBACK_HXX
#define MPD_NFS_CALLBACK_HXX

#include <exception>

/**
 * Callbacks for an asynchronous libnfs operation.
 *
 * Note that no callback is invoked for cancelled operations.
 */
class NfsCallback {
public:
	/**
	 * The operation completed successfully.
	 */
	virtual void OnNfsCallback(unsigned status, void *data) noexcept = 0;

	/**
	 * An error has occurred.
	 */
	virtual void OnNfsError(std::exception_ptr &&e) noexcept = 0;
};

#endif
