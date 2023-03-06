// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "LogListener.hxx"
#include "java/Class.hxx"
#include "java/String.hxx"

LogListener::LogListener(JNIEnv *env, jobject obj) noexcept
	:Java::GlobalObject(env, obj)
{
	Java::Class cls(env, env->GetObjectClass(Get()));

	onLogMethod = env->GetMethodID(cls, "onLog", "(ILjava/lang/String;)V");
	assert(onLogMethod);
}

void
LogListener::OnLog(JNIEnv *env, int priority, const char *msg) const noexcept
{
	assert(env != nullptr);

	env->CallVoidMethod(Get(), onLogMethod, priority,
			    Java::String(env, msg).Get());
}
