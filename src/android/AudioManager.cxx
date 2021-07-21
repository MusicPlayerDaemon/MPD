/*
 * Copyright 2003-2021 The Music Player Daemon Project
 * http://www.musicpd.org
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

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
