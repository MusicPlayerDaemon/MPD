// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include "UniqueHandle.hxx"

#include <cassert>
#include <exception>
#include <optional>
#include <utility>

namespace Co {

/**
 * Task type for a coroutine that generates values.
 *
 * It is meant to be used in a range-based "for" loop.
 */
template<typename T>
class Generator {
public:
	struct promise_type {
		std::optional<T> value;
		std::exception_ptr error;

		bool IsReady() const noexcept {
			return value || error;
		}

		[[nodiscard]]
		Generator get_return_object() noexcept {
			return Generator(std::coroutine_handle<promise_type>::from_promise(*this));
		}

		auto initial_suspend() noexcept {
			/* don't suspend initially because we want the
			   first value to be available immediately, or
			   else operator*() below does not work */
			return std::suspend_never{};
		}

		auto final_suspend() noexcept {
			return std::suspend_always{};
		}

		void unhandled_exception() noexcept {
			error = std::current_exception();
		}

		template<typename U>
		std::suspend_always yield_value(U &&_value) {
			value.emplace(std::forward<U>(_value));
			return {};
		}

		void return_void() noexcept {}
	};

private:
	UniqueHandle<promise_type> coroutine;

	[[nodiscard]]
	explicit Generator(std::coroutine_handle<promise_type> _coroutine) noexcept
		:coroutine(_coroutine)
	{
	}

	struct end_iterator {};

	struct iterator {
		const std::coroutine_handle<promise_type> co;

		bool operator==(const end_iterator &) const noexcept {
			return co.done() && !co.promise().IsReady();
		}

		iterator &operator++() {
			auto &promise = co.promise();
			assert(!promise.error);
			assert(promise.value);

			promise.value.reset();
			co.resume();
			return *this;
		}

		T &&operator*() const {
			auto &promise = co.promise();
			assert(promise.IsReady());

			if (promise.error)
				std::rethrow_exception(co.promise().error);

			return std::move(*promise.value);
		}
	};

public:
	auto begin() const noexcept {
		return iterator{coroutine.get()};
	}

	auto end() const noexcept{
		return end_iterator{};
	}
};

} // namespace Co
