/*
 * Copyright 2016-2021 Max Kellermann <max.kellermann@gmail.com>
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

#pragma once

#include <type_traits>
#include <utility>

/**
 * This object stores a function pointer wrapping a method, and a
 * reference to an instance of the method's class.  It can be used to
 * wrap instance methods as callback functions.
 *
 * @param S the plain function signature type
 */
template<typename S=void()>
class BoundMethod;

template<typename R,
	bool NoExcept,
	typename... Args>
class BoundMethod<R(Args...) noexcept(NoExcept)> {
	typedef R (*function_pointer)(void *instance, Args... args) noexcept(NoExcept);

	void *instance_;
	function_pointer function;

public:
	/**
	 * Non-initializing trivial constructor
	 */
	BoundMethod() = default;

	constexpr
	BoundMethod(void *_instance, function_pointer _function) noexcept
		:instance_(_instance), function(_function) {}

	/**
	 * Construct an "undefined" object.  It must not be called,
	 * and its "bool" operator returns false.
	 */
	BoundMethod(std::nullptr_t) noexcept:function(nullptr) {}

	/**
	 * Was this object initialized with a valid function pointer?
	 */
	operator bool() const noexcept {
		return function != nullptr;
	}

	R operator()(Args... args) const {
		return function(instance_, std::forward<Args>(args)...);
	}
};

namespace BindMethodDetail {

/**
 * Helper class which introspects a method/function pointer type.
 *
 * @param M the method/function pointer type
 */
template<typename M>
struct SignatureHelper;

template<typename R, bool NoExcept, typename T, typename... Args>
struct SignatureHelper<R (T::*)(Args...) noexcept(NoExcept)> {
	/**
	 * The class which contains the given method (signature).
	 */
	typedef T class_type;

	/**
	 * A function type which describes the "plain" function
	 * signature.
	 */
	typedef R plain_signature(Args...) noexcept(NoExcept);

	typedef R (*function_pointer)(void *instance,
				      Args...) noexcept(NoExcept);
};

template<typename R, bool NoExcept, typename... Args>
struct SignatureHelper<R (*)(Args...) noexcept(NoExcept)> {
	typedef R plain_signature(Args...) noexcept(NoExcept);

	typedef R (*function_pointer)(void *instance,
				      Args...) noexcept(NoExcept);
};

/**
 * Generate a wrapper function.
 *
 * @param method the method/function pointer
 */
template<typename M, auto method>
struct WrapperGenerator;

template<typename T, bool NoExcept,
	 auto method, typename R, typename... Args>
struct WrapperGenerator<R (T::*)(Args...) noexcept(NoExcept), method> {
	static R Invoke(void *_instance, Args... args) noexcept(NoExcept) {
		auto &t = *(T *)_instance;
		return (t.*method)(std::forward<Args>(args)...);
	}
};

template<auto function, bool NoExcept, typename R, typename... Args>
struct WrapperGenerator<R (*)(Args...) noexcept(NoExcept), function> {
	static R Invoke(void *, Args... args) noexcept(NoExcept) {
		return function(std::forward<Args>(args)...);
	}
};

template<auto method>
typename SignatureHelper<decltype(method)>::function_pointer
MakeWrapperFunction() noexcept
{
	return WrapperGenerator<decltype(method), method>::Invoke;
}

} /* namespace BindMethodDetail */

/**
 * Construct a #BoundMethod instance.
 *
 * @param method the method pointer
 * @param instance the instance of #T to be bound
 */
template<auto method>
constexpr auto
BindMethod(typename BindMethodDetail::SignatureHelper<decltype(method)>::class_type &instance) noexcept
{
	using H = BindMethodDetail::SignatureHelper<decltype(method)>;
	using plain_signature = typename H::plain_signature;
	return BoundMethod<plain_signature>{
		&instance,
		BindMethodDetail::MakeWrapperFunction<method>(),
	};
}

/**
 * Shortcut macro which takes an instance and a method pointer and
 * constructs a #BoundMethod instance.
 */
#define BIND_METHOD(instance, method) \
	BindMethod<method>(instance)

/**
 * Shortcut wrapper for BIND_METHOD() which assumes "*this" is the
 * instance to be bound.
 */
#define BIND_THIS_METHOD(method) BIND_METHOD(*this, &std::remove_reference_t<decltype(*this)>::method)

/**
 * Construct a #BoundMethod instance for a plain function.
 *
 * @param function the function pointer
 */
template<auto function>
constexpr auto
BindFunction() noexcept
{
	using H = BindMethodDetail::SignatureHelper<decltype(function)>;
	using plain_signature = typename H::plain_signature;
	return BoundMethod<plain_signature>{
		nullptr,
		BindMethodDetail::MakeWrapperFunction<function>(),
	};
}

/**
 * Shortcut macro which takes a function pointer and constructs a
 * #BoundMethod instance.
 */
#define BIND_FUNCTION(function) \
	BindFunction<&function>()
