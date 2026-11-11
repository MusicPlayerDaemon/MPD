// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "AudioFocus.hxx"
#include "java/Class.hxx"
#include "java/Exception.hxx"

#include <cassert>

AudioFocus::AudioFocus(JNIEnv *env, jobject obj) noexcept
	:Java::GlobalObject(env, obj)
{
	Java::Class cls(env, env->GetObjectClass(Get()));

	request_method = env->GetMethodID(cls, "requestAudioFocus", "()V");
	assert(request_method);

	abandon_method = env->GetMethodID(cls, "abandonAudioFocus", "()V");
	assert(abandon_method);
}

void
AudioFocus::Request(JNIEnv *env) const noexcept
{
	assert(env != nullptr);

	env->CallVoidMethod(Get(), request_method);
	Java::DiscardException(env);
}

void
AudioFocus::Abandon(JNIEnv *env) const noexcept
{
	assert(env != nullptr);

	env->CallVoidMethod(Get(), abandon_method);
	Java::DiscardException(env);
}
