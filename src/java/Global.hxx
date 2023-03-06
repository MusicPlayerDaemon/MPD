// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#ifndef JAVA_GLOBAL_HXX
#define JAVA_GLOBAL_HXX

#include <jni.h>

namespace Java {

extern JavaVM *jvm;

void Init(JNIEnv *env) noexcept;

static inline void
DetachCurrentThread() noexcept
{
	if (jvm != nullptr)
		jvm->DetachCurrentThread();
}

[[gnu::pure]]
static inline JNIEnv *
GetEnv() noexcept
{
	JNIEnv *env;
	jvm->AttachCurrentThread(&env, nullptr);
	return env;
}

} // namespace Java

#endif
