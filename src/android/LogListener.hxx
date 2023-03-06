// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_ANDROID_LOG_LISTENER_HXX
#define MPD_ANDROID_LOG_LISTENER_HXX

#include "java/Object.hxx"

class LogListener : public Java::GlobalObject {
	jmethodID onLogMethod;

public:
	LogListener(JNIEnv *env, jobject obj) noexcept;

	void OnLog(JNIEnv *env, int priority, const char *msg) const noexcept;
};

#endif
