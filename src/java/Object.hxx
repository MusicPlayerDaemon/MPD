// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#ifndef JAVA_OBJECT_HXX
#define JAVA_OBJECT_HXX

#include "Ref.hxx"

#include <jni.h>

#include <cassert>

namespace Java {

/**
 * Wrapper for a local "jobject" reference.
 */
using LocalObject = LocalRef<jobject>;

using GlobalObject = GlobalRef<jobject>;

/**
 * Utilities for java.net.Object.
 */
class Object {
	static jmethodID toString_method;

public:
	static void Initialise(JNIEnv *env);

	static jstring toString(JNIEnv *env, jobject o) {
		assert(env != nullptr);
		assert(o != nullptr);
		assert(toString_method != nullptr);

		return (jstring)env->CallObjectMethod(o, toString_method);
	}
};

} // namespace Java

#endif
