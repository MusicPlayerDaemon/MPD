// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include "java/Object.hxx"

/**
 * Wrapper for a Java object implementing the interface
 * org.musicpd.Bridge.AudioFocus.
 */
class AudioFocus : public Java::GlobalObject {
	jmethodID request_method, abandon_method;

public:
	AudioFocus(JNIEnv *env, jobject obj) noexcept;

	/**
	 * Request the audio focus for media playback.  This may be
	 * called from any thread.
	 */
	void Request(JNIEnv *env) const noexcept;

	/**
	 * Abandon the audio focus.  This may be called from any
	 * thread.
	 */
	void Abandon(JNIEnv *env) const noexcept;
};
