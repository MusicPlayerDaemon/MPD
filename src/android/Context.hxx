// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_ANDROID_CONTEXT_HXX
#define MPD_ANDROID_CONTEXT_HXX

#include "java/Object.hxx"

class AllocatedPath;
class AudioManager;

class Context : public Java::GlobalObject {
public:
	/**
	 * Global initialisation.  Looks up the methods of the
	 * Context Java class.
	 */
	static void Initialise(JNIEnv *env) noexcept;

	Context(JNIEnv *env, jobject obj) noexcept
		:Java::GlobalObject(env, obj) {}

	/**
	 * @param type the subdirectory name; may be nullptr
	 */
	[[gnu::pure]]
	AllocatedPath GetExternalFilesDir(JNIEnv *env,
					  const char *type=nullptr) noexcept;

	[[gnu::pure]]
	AllocatedPath GetCacheDir(JNIEnv *env) const noexcept;

	[[gnu::pure]]
	AudioManager *GetAudioManager(JNIEnv *env) noexcept;
};

#endif
