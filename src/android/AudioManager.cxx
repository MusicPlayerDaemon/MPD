// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "AudioManager.hxx"
#include "java/Class.hxx"
#include "java/Exception.hxx"
#include "java/File.hxx"

#define STREAM_MUSIC 3

AudioManager::AudioManager(JNIEnv *env, jobject obj) noexcept
	: Java::GlobalObject(env, obj)
{
	Java::Class cls(env, env->GetObjectClass(Get()));
	jmethodID method = env->GetMethodID(cls, "getStreamMaxVolume", "(I)I");
	assert(method);
	maxVolume = env->CallIntMethod(Get(), method, STREAM_MUSIC);

	getStreamVolumeMethod = env->GetMethodID(cls, "getStreamVolume", "(I)I");
	assert(getStreamVolumeMethod);

	setStreamVolumeMethod = env->GetMethodID(cls, "setStreamVolume", "(III)V");
	assert(setStreamVolumeMethod);
}

int
AudioManager::GetVolume(JNIEnv *env)
{
	if (maxVolume == 0)
		return 0;
	return env->CallIntMethod(Get(), getStreamVolumeMethod, STREAM_MUSIC);
}

void
AudioManager::SetVolume(JNIEnv *env, int volume)
{
	if (maxVolume == 0)
		return;
	env->CallVoidMethod(Get(), setStreamVolumeMethod, STREAM_MUSIC, volume, 0);
}
