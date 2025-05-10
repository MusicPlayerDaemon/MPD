// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_REMOTE_TAG_SCANNER_HXX
#define MPD_REMOTE_TAG_SCANNER_HXX

#include <exception>

struct Tag;

/**
 * Handler for the #RemoteTagScanner result.  It will call one of the
 * methods upon completion.  Must be thread-safe.
 */
class RemoteTagHandler {
public:
	/**
	 * Called on success.
	 */
	virtual void OnRemoteTag(Tag &&tag) noexcept = 0;

	/**
	 * Called on error.
	 */
	virtual void OnRemoteTagError(std::exception_ptr e) noexcept = 0;
};

/**
 * This class can load tags of a remote file.  It is created by
 * InputPlugin::scan_tags(), and the #RemoteTagHandler will be called
 * upon completion.
 *
 * To start the operation, call Start().
 *
 * You can cancel the operation at any time by destructing this
 * object; after successful cancellation, the handler will not be
 * invoked, though it cannot be guaranteed that the handler is not
 * already being called in another thread.
 */
class RemoteTagScanner {
public:
	virtual ~RemoteTagScanner() noexcept = default;
	virtual void Start() = 0;

	virtual bool DisableTagCaching ()
	{
		return false;
	}
};

#endif
