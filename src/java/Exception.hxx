// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#ifndef JAVA_EXCEPTION_HXX
#define JAVA_EXCEPTION_HXX

#include <stdexcept>

#include <jni.h>

namespace Java {

class Exception : public std::runtime_error {
public:
	explicit Exception(JNIEnv *env, jthrowable e) noexcept;
};

/**
 * Check if a Java exception has occurred, and if yes, convert
 * it to a C++ #Exception and throw that.
 */
void RethrowException(JNIEnv *env);

/**
 * Check if an exception has occurred, and discard it.
 *
 * @return true if an exception was found (and discarded)
 */
static inline bool DiscardException(JNIEnv *env) noexcept {
	bool result = env->ExceptionCheck();
	if (result)
		env->ExceptionClear();
	return result;
}

} // namespace Java

#endif
