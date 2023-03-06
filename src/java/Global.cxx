// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "Global.hxx"

namespace Java {

JavaVM *jvm;

void Init(JNIEnv *env) noexcept
{
	env->GetJavaVM(&jvm);
}

} // namespace Java
