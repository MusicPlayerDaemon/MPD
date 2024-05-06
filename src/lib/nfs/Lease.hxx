// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include "util/IntrusiveList.hxx"

#include <exception>

class NfsLease : public IntrusiveListHook<> {
public:
	/**
	 * The #NfsConnection has successfully mounted the server's
	 * export and is ready for regular operation.
	 */
	virtual void OnNfsConnectionReady() noexcept = 0;

	/**
	 * The #NfsConnection has failed to mount the server's export.
	 * This is being called instead of OnNfsConnectionReady().
	 */
	virtual void OnNfsConnectionFailed(std::exception_ptr e) noexcept = 0;

	/**
	 * The #NfsConnection has failed after OnNfsConnectionReady()
	 * had been called already.
	 */
	virtual void OnNfsConnectionDisconnected(std::exception_ptr e) noexcept = 0;
};
