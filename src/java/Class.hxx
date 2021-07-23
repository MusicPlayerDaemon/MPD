/*
 * Copyright 2010-2021 Max Kellermann <max.kellermann@gmail.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

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
