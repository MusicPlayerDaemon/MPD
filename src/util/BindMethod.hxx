/*
 * Copyright (C) 2016 Max Kellermann <max.kellermann@gmail.com>
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

#ifndef BIND_METHOD_HXX
#define BIND_METHOD_HXX

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

template<typename R, typename... Args>
class BoundMethod<R(Args...)> {
	typedef R (*function_pointer)(void *instance, Args... args);

	void *instance_;
	function_pointer function;

public:
	/**
	 * Non-initializing trivial constructor
	 */
	BoundMethod() = default;

	constexpr
	BoundMethod(void *_instance, function_pointer _function)
		:instance_(_instance), function(_function) {}

	/**
	 * Construct an "undefined" object.  It must not be called,
	 * and its "bool" operator returns false.
	 */
	BoundMethod(std::nullptr_t):function(nullptr) {}

	/**
	 * Was this object initialized with a valid function pointer?
	 */
	operator bool() const {
		return function != nullptr;
	}

	R operator()(Args... args) const {
		return function(instance_, std::forward<Args>(args)...);
	}
};

namespace BindMethodDetail {

/**
 * Helper class which converts a signature type to a method pointer
 * type.
 *
 * @param T the wrapped class
 * @param S the function signature type (plain, without instance
 * pointer)
 */
template<typename T, typename S>
struct MethodWithSignature;

template<typename T, typename R, typename... Args>
struct MethodWithSignature<T, R(Args...)> {
	typedef R (T::*method_pointer)(Args...);
};

/**
 * Helper class which introspects a method pointer type.
 *
 * @param M the method pointer type
 */
template<typename M>
struct MethodSignatureHelper;

template<typename R, typename T, typename... Args>
struct MethodSignatureHelper<R (T::*)(Args...)> {
	/**
	 * The class which contains the given method (signature).
	 */
	typedef T class_type;

	/**
	 * A function type which describes the "plain" function
	 * signature.
	 */
	typedef R plain_signature(Args...);
};

/**
 * Helper class which converts a plain function signature type to a
 * wrapper function pointer type.
 */
template<typename S>
struct MethodWrapperWithSignature;

template<typename R, typename... Args>
struct MethodWrapperWithSignature<R(Args...)> {
	typedef R (*function_pointer)(void *instance, Args...);
};

/**
 * Generate a wrapper function.  Helper class for
 * #BindMethodWrapperGenerator.
 *
 * @param T the containing class
 * @param M the method pointer type
 * @param method the method pointer
 * @param R the return type
 * @param Args the method arguments
 */
template<typename T, typename M, M method, typename R, typename... Args>
struct BindMethodWrapperGenerator2 {
	static R Invoke(void *_instance, Args... args) {
		auto &t = *(T *)_instance;
		return (t.*method)(std::forward<Args>(args)...);
	}
};

/**
 * Generate a wrapper function.
 *
 * @param T the containing class
 * @param M the method pointer type
 * @param method the method pointer
 * @param S the plain function signature type
 */
template<typename T, typename M, M method, typename S>
struct BindMethodWrapperGenerator;

template<typename T, typename M, M method, typename R, typename... Args>
struct BindMethodWrapperGenerator<T, M, method, R(Args...)>
	: BindMethodWrapperGenerator2<T, M, method, R, Args...> {
};

template<typename T, typename S,
	 typename MethodWithSignature<T, S>::method_pointer method>
typename MethodWrapperWithSignature<S>::function_pointer
MakeBindMethodWrapper()
{
	return BindMethodWrapperGenerator<T, typename MethodWithSignature<T, S>::method_pointer, method, S>::Invoke;
}

} /* namespace BindMethodDetail */

/**
 * Construct a #BoundMethod instance.
 *
 * @param T the containing class
 * @param S the plain function signature type
 * @param method the method pointer
 * @param instance the instance of #T to be bound
 */
template<typename T, typename S,
	 typename BindMethodDetail::MethodWithSignature<T, S>::method_pointer method>
constexpr BoundMethod<S>
BindMethod(T &_instance)
{
	return BoundMethod<S>(&_instance,
			      BindMethodDetail::MakeBindMethodWrapper<T, S, method>());
}

/**
 * Shortcut macro which takes an instance and a method pointer and
 * constructs a #BoundMethod instance.
 */
#define BIND_METHOD(instance, method) \
	BindMethod<typename BindMethodDetail::MethodSignatureHelper<decltype(method)>::class_type, \
		   typename BindMethodDetail::MethodSignatureHelper<decltype(method)>::plain_signature, \
		   method>(instance)

/**
 * Shortcut wrapper for BIND_METHOD() which assumes "*this" is the
 * instance to be bound.
 */
#define BIND_THIS_METHOD(method) BIND_METHOD(*this, &std::remove_reference<decltype(*this)>::type::method)

#endif
