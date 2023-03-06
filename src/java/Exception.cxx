// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "Exception.hxx"
#include "Object.hxx"
#include "String.hxx"

Java::Exception::Exception(JNIEnv *env, jthrowable e) noexcept
	:std::runtime_error(Java::String(env, Object::toString(env, e)).ToString())
{
}

void
Java::RethrowException(JNIEnv *env)
{
	LocalRef<jthrowable> exception{env, env->ExceptionOccurred()};
	if (!exception)
		return;

	env->ExceptionClear();
	throw Exception(env, exception);
}
