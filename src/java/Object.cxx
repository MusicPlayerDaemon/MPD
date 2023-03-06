// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "Object.hxx"
#include "Class.hxx"

jmethodID Java::Object::toString_method;

void
Java::Object::Initialise(JNIEnv *env)
{
	assert(env != nullptr);

	Class cls(env, "java/lang/Object");

	toString_method = env->GetMethodID(cls, "toString",
					   "()Ljava/lang/String;");
	assert(toString_method != nullptr);
}
