/*
 * Copyright 2010-2018 Max Kellermann <max.kellermann@gmail.com>
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

#ifndef JAVA_REF_HXX
#define JAVA_REF_HXX

#include "Global.hxx"

#include <jni.h>

#include <assert.h>

namespace Java {
	/**
	 * Hold a local reference on a JNI object.
	 */
	template<typename T>
	class LocalRef {
		JNIEnv *const env;
		const T value;

	public:
		/**
		 * The local reference is obtained by the caller.  May
		 * be nullptr.
		 */
		LocalRef(JNIEnv *_env, T _value) noexcept
			:env(_env), value(_value)
		{
			assert(env != nullptr);
		}

		~LocalRef() noexcept {
			env->DeleteLocalRef(value);
		}

		LocalRef(const LocalRef &other) = delete;
		LocalRef &operator=(const LocalRef &other) = delete;

		JNIEnv *GetEnv() const noexcept {
			return env;
		}

		operator bool() const noexcept {
			return value != nullptr;
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
		/**
		 * Constructs an uninitialized object.  The method
		 * set() must be called before it is destructed.
		 */
		GlobalRef() = default;

		GlobalRef(JNIEnv *env, T _value) noexcept
			:value(_value)
		{
			assert(env != nullptr);
			assert(value != nullptr);

			value = (T)env->NewGlobalRef(value);
		}

		~GlobalRef() noexcept {
			GetEnv()->DeleteGlobalRef(value);
		}

		GlobalRef(const GlobalRef &other) = delete;
		GlobalRef &operator=(const GlobalRef &other) = delete;

		/**
		 * Sets the object, ignoring the previous value.  This
		 * is only allowed once after the default constructor
		 * was used.
		 */
		void Set(JNIEnv *env, T _value) noexcept {
			assert(_value != nullptr);

			value = (T)env->NewGlobalRef(_value);
		}

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
}

#endif
