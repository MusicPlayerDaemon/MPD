// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#ifndef JAVA_REF_HXX
#define JAVA_REF_HXX

#include "Global.hxx"

#include <jni.h>

#include <cassert>
#include <utility>

namespace Java {

/**
 * Hold a local reference on a JNI object.
 */
template<typename T>
class LocalRef {
	JNIEnv *env;
	T value = nullptr;

public:
	LocalRef() noexcept = default;
	LocalRef(std::nullptr_t) noexcept {}

	/**
	 * The local reference is obtained by the caller.  May
	 * be nullptr.
	 */
	LocalRef(JNIEnv *_env, T _value) noexcept
		:env(_env), value(_value)
	{
		assert(env != nullptr);
	}

	LocalRef(LocalRef &&src) noexcept
		:env(src.env),
		 value(std::exchange(src.value, nullptr)) {}

	~LocalRef() noexcept {
		if (value != nullptr)
			env->DeleteLocalRef(value);
	}

	LocalRef &operator=(LocalRef &&src) noexcept {
		using std::swap;
		swap(env, src.env);
		swap(value, src.value);
		return *this;
	}

	JNIEnv *GetEnv() const noexcept {
		return env;
	}

	operator bool() const noexcept {
		return value != nullptr;
	}

	bool operator==(std::nullptr_t n) const noexcept {
		return value == n;
	}

	bool operator!=(std::nullptr_t n) const noexcept {
		return !(*this == n);
	}

	T Get() const noexcept {
		return value;
	}

	operator T() const noexcept {
		return value;
	}
};

/**
 * Hold a global reference on a JNI object.
 */
template<typename T>
class GlobalRef {
	T value;

public:
	GlobalRef(JNIEnv *env, T _value) noexcept
		:value(_value)
	{
		assert(env != nullptr);
		assert(value != nullptr);

		value = (T)env->NewGlobalRef(value);
	}

	GlobalRef(const LocalRef<T> &src) noexcept
		:GlobalRef(src.GetEnv(), src.Get()) {}

	~GlobalRef() noexcept {
		GetEnv()->DeleteGlobalRef(value);
	}

	GlobalRef(const GlobalRef &other) = delete;
	GlobalRef &operator=(const GlobalRef &other) = delete;

	T Get() const noexcept {
		return value;
	}

	operator T() const noexcept {
		return value;
	}
};

/**
 * Container for a global reference to a JNI object that gets
 * initialised and deinitialised explicitly.  Since there is
 * no implicit initialisation in the default constructor, this
 * is a trivial C++ class.  It should only be used for global
 * variables that are implicitly initialised with zeroes.
 */
template<typename T>
class TrivialRef {
	T value;

public:
	TrivialRef() = default;

	TrivialRef(const TrivialRef &other) = delete;
	TrivialRef &operator=(const TrivialRef &other) = delete;

	bool IsDefined() const noexcept {
		return value != nullptr;
	}

	/**
	 * Obtain a global reference on the specified object
	 * and store it.  This object must not be set already.
	 */
	void Set(JNIEnv *env, T _value) noexcept {
		assert(value == nullptr);
		assert(_value != nullptr);

		value = (T)env->NewGlobalRef(_value);
	}

	/**
	 * Release the global reference and clear this object.
	 */
	void Clear(JNIEnv *env) noexcept {
		assert(value != nullptr);

		env->DeleteGlobalRef(value);
		value = nullptr;
	}

	/**
	 * Release the global reference and clear this object.
	 * It is allowed to call this method without ever
	 * calling Set().
	 */
	void ClearOptional(JNIEnv *env) noexcept {
		if (value != nullptr)
			Clear(env);
	}

	T Get() const noexcept {
		return value;
	}

	operator T() const noexcept {
		return value;
	}
};

} // namespace Java

#endif
