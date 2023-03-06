// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#ifndef JAVA_CLASS_HXX
#define JAVA_CLASS_HXX

#include "Ref.hxx"
#include "Exception.hxx"

#include <cassert>

namespace Java {

/**
 * Wrapper for a local "jclass" reference.
 */
class Class : public LocalRef<jclass> {
public:
	Class(JNIEnv *env, jclass cls) noexcept
		:LocalRef<jclass>(env, cls) {}

	Class(JNIEnv *env, const char *name) noexcept
		:LocalRef<jclass>(env, env->FindClass(name)) {}
};

/**
 * Wrapper for a global "jclass" reference.
 */
class TrivialClass : public TrivialRef<jclass> {
public:
	void Find(JNIEnv *env, const char *name) noexcept {
		assert(env != nullptr);
		assert(name != nullptr);

		const Java::Class cls{env, env->FindClass(name)};
		assert(cls != nullptr);

		Set(env, cls);
	}

	bool FindOptional(JNIEnv *env, const char *name) noexcept {
		assert(env != nullptr);
		assert(name != nullptr);

		const Java::Class cls{env, env->FindClass(name)};
		if (DiscardException(env))
			return false;

		Set(env, cls);
		return true;
	}
};

} // namespace Java

#endif
