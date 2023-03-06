// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#ifndef JAVA_FILE_HXX
#define JAVA_FILE_HXX

#include "Object.hxx"
#include "String.hxx"

#include <jni.h>

class AllocatedPath;

namespace Java {

/**
 * Wrapper for a java.io.File object.
 */
class File : public LocalObject {
	static jmethodID getAbsolutePath_method;

public:
	using LocalObject::LocalObject;

	[[gnu::nonnull]]
	static void Initialise(JNIEnv *env) noexcept;

	[[gnu::nonnull]]
	static jstring GetAbsolutePath(JNIEnv *env, jobject file) noexcept {
		return (jstring)env->CallObjectMethod(file,
						      getAbsolutePath_method);
	}

	String GetAbsolutePath() const noexcept {
		return {GetEnv(), GetAbsolutePath(GetEnv(), Get())};
	}

	String GetAbsolutePathChecked() const noexcept {
		return *this ? GetAbsolutePath() : nullptr;
	}

	/**
	 * Invoke File.getAbsolutePath() and release the
	 * specified File reference.
	 */
	[[gnu::pure]] [[gnu::nonnull]]
	static AllocatedPath ToAbsolutePath(JNIEnv *env,
					    jobject file) noexcept;
};

} // namespace Java

#endif
