// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_INPUT_STREAM_HANDLER_HXX
#define MPD_INPUT_STREAM_HANDLER_HXX

/**
 * An interface which gets receives events from an #InputStream.  Its
 * methods will be called from within an arbitrary thread and must not
 * block.
 *
 * A reference to an instance is passed to the #InputStream, but it
 * remains owned by the caller.
 */
class InputStreamHandler {
public:
	/**
	 * Called when InputStream::IsReady() becomes true.
	 *
	 * Before querying metadata from the #InputStream,
	 * InputStream::Update() must be called.
	 *
	 * Caller locks InputStream::mutex.
	 */
	virtual void OnInputStreamReady() noexcept = 0;

	/**
	 * Called when InputStream::IsAvailable() becomes true.
	 *
	 * Caller locks InputStream::mutex.
	 */
	virtual void OnInputStreamAvailable() noexcept = 0;
};

#endif
