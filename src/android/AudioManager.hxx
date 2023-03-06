// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_ANDROID_AUDIO_MANAGER_HXX
#define MPD_ANDROID_AUDIO_MANAGER_HXX

#include "java/Object.hxx"

class AudioManager : public Java::GlobalObject {
	int maxVolume;
	jmethodID getStreamVolumeMethod;
	jmethodID setStreamVolumeMethod;

public:
	AudioManager(JNIEnv *env, jobject obj) noexcept;

	int GetMaxVolume() { return maxVolume; }
	int GetVolume(JNIEnv *env);
	void SetVolume(JNIEnv *env, int);
};

#endif
