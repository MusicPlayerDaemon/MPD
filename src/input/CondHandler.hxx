// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_COND_INPUT_STREAM_HANDLER_HXX
#define MPD_COND_INPUT_STREAM_HANDLER_HXX

#include "Handler.hxx"
#include "thread/Cond.hxx"

/**
 * An #InputStreamHandler implementation which signals a #Cond.
 */
struct CondInputStreamHandler final : InputStreamHandler {
	Cond cond;

	/* virtual methods from class InputStreamHandler */
	void OnInputStreamReady() noexcept override {
		cond.notify_one();
	}

	void OnInputStreamAvailable() noexcept override {
		cond.notify_one();
	}
};

#endif
