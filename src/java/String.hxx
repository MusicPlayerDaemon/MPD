// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#ifndef JAVA_STRING_HXX
#define JAVA_STRING_HXX

#include "Ref.hxx"

#include <jni.h>

#include <cstddef>
#include <string>
#include <string_view>

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

	String(JNIEnv *_env, std::string_view _value) noexcept;

	/**
	 * This constructor allows passing a nullptr value, which maps
	 * to a "null" in Java.
	 */
	static String Optional(JNIEnv *_env, const char *_value) noexcept {
		return _value != nullptr
			? String{_env, _value}
			: String{};
	}

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
