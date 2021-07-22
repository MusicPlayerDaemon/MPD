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

#ifndef JAVA_STRING_HXX
#define JAVA_STRING_HXX

#include "Ref.hxx"

#include <jni.h>

#include <cstddef>
#include <string>

namespace Java {

class StringUTFChars {
	JNIEnv *env;
	jstring string;
	const char *chars = nullptr;

public:
	StringUTFChars() noexcept = default;
	StringUTFChars(std::nullptr_t) noexcept {}

	StringUTFChars(JNIEnv *_env,
		       const jstring _string,
		       const char *const _chars) noexcept
		:env(_env), string(_string), chars(_chars) {}

	StringUTFChars(StringUTFChars &&src) noexcept
		:env(src.env), string(src.string),
		 chars(std::exchange(src.chars, nullptr)) {}

	~StringUTFChars() noexcept {
		if (chars != nullptr)
			env->ReleaseStringUTFChars(string, chars);
	}

	StringUTFChars &operator=(StringUTFChars &&src) noexcept {
		using std::swap;
		swap(env, src.env);
		swap(string, src.string);
		swap(chars, src.chars);
		return *this;
	}

	const char *c_str() const noexcept {
		return chars;
	}

	operator bool() const noexcept {
		return chars != nullptr;
	}
};

/**
 * Wrapper for a local "jstring" reference.
 */
class String : public LocalRef<jstring> {
public:
	using LocalRef::LocalRef;

	String(JNIEnv *_env, const char *_value) noexcept
		:LocalRef<jstring>(_env, _env->NewStringUTF(_value)) {}

	static StringUTFChars GetUTFChars(JNIEnv *env, jstring s) noexcept {
		return {env, s, env->GetStringUTFChars(s, nullptr)};
	}

	StringUTFChars GetUTFChars() const noexcept {
		return GetUTFChars(GetEnv(), Get());
	}

	/**
	 * Copy the value to the specified buffer.  Truncates
	 * the value if it does not fit into the buffer.
	 *
	 * @return a pointer to the terminating null byte,
	 * nullptr on error
	 */
	static char *CopyTo(JNIEnv *env, jstring value,
			    char *buffer, size_t max_size) noexcept;

	/**
	 * Copy the value to the specified buffer.  Truncates
	 * the value if it does not fit into the buffer.
	 *
	 * @return a pointer to the terminating null byte,
	 * nullptr on error
	 */
	char *CopyTo(char *buffer, size_t max_size) const noexcept {
		return CopyTo(GetEnv(), Get(), buffer, max_size);
	}

	static std::string ToString(JNIEnv *env, jstring s) noexcept;

	std::string ToString() const noexcept {
		return ToString(GetEnv(), Get());
	}
};

} // namespace Java

#endif
