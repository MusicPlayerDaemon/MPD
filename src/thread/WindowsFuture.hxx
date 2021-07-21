/*
 * Copyright 2020-2021 The Music Player Daemon Project
 * http://www.musicpd.org
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef THREAD_WINDOWS_FUTURE_HXX
#define THREAD_WINDOWS_FUTURE_HXX

#include "CriticalSection.hxx"
#include "WindowsCond.hxx"

#include <memory>
#include <variant>

enum class WinFutureErrc : int {
	future_already_retrieved = 1,
	promise_already_satisfied,
	no_state,
	broken_promise,
};

enum class WinFutureStatus { ready, timeout, deferred };

static inline const std::error_category &win_future_category() noexcept;
class WinFutureCategory : public std::error_category {
public:
	const char *name() const noexcept override { return "win_future"; }
	std::string message(int Errcode) const override {
		using namespace std::literals;
		switch (static_cast<WinFutureErrc>(Errcode)) {
		case WinFutureErrc::broken_promise:
			return "Broken promise"s;
		case WinFutureErrc::future_already_retrieved:
			return "Future already retrieved"s;
		case WinFutureErrc::promise_already_satisfied:
			return "Promise already satisfied"s;
		case WinFutureErrc::no_state:
			return "No associated state"s;
		default:
			return "Unknown error"s;
		}
	}
	std::error_condition default_error_condition(int code) const noexcept override {
		return std::error_condition(code, win_future_category());
	}
};
static inline const std::error_category &win_future_category() noexcept {
	static const WinFutureCategory win_future_category_instance{};
	return win_future_category_instance;
}

class WinFutureError : public std::logic_error {
public:
	WinFutureError(WinFutureErrc errcode)
	: WinFutureError(
		  std::error_code(static_cast<int>(errcode), win_future_category())) {}

private:
	explicit WinFutureError(std::error_code errcode)
	: std::logic_error("WinFutureError: " + errcode.message()), code(errcode) {}
	std::error_code code;
};

template <typename T>
class WinFutureState {
private:
	mutable CriticalSection mutex;
	WindowsCond condition;
	std::variant<std::monostate, T, std::exception_ptr> result;
	bool retrieved = false;
	bool ready = false;

public:
	bool is_ready() const noexcept {
		std::unique_lock<CriticalSection> lock(mutex);
		return ready;
	}

	bool already_retrieved() const noexcept {
		std::unique_lock<CriticalSection> lock(mutex);
		return retrieved;
	}

	void wait() {
		std::unique_lock<CriticalSection> lock(mutex);
		condition.wait(lock, [this]() { return ready; });
	}

	template <class Rep, class Period>
	WinFutureStatus
	wait_for(const std::chrono::duration<Rep, Period> &timeout_duration) const {
		std::unique_lock<CriticalSection> lock(mutex);
		// deferred function not support yet
		if (condition.wait_for(lock, timeout_duration,
				       [this]() { return ready; })) {
			return WinFutureStatus::ready;
		}
		return WinFutureStatus::timeout;
	}

	virtual T &get_value() {
		std::unique_lock<CriticalSection> lock(mutex);
		if (retrieved) {
			throw WinFutureError(WinFutureErrc::future_already_retrieved);
		}
		if (auto eptr = std::get_if<std::exception_ptr>(&result)) {
			std::rethrow_exception(*eptr);
		}
		retrieved = true;
		condition.wait(lock, [this]() { return ready; });
		if (auto eptr = std::get_if<std::exception_ptr>(&result)) {
			std::rethrow_exception(*eptr);
		}
		return *std::get_if<T>(&result);
	}

	void set_value(const T &value) {
		std::unique_lock<CriticalSection> lock(mutex);
		if (!std::holds_alternative<std::monostate>(&result)) {
			throw WinFutureError(WinFutureErrc::promise_already_satisfied);
		}
		result.template emplace<T>(value);
		ready = true;
		condition.notify_all();
	}

	void set_value(T &&value) {
		std::unique_lock<CriticalSection> lock(mutex);
		if (!std::holds_alternative<std::monostate>(result)) {
			throw WinFutureError(WinFutureErrc::promise_already_satisfied);
		}
		result.template emplace<T>(std::move(value));
		ready = true;
		condition.notify_all();
	}

	void set_exception(std::exception_ptr eptr) {
		std::unique_lock<CriticalSection> lock(mutex);
		if (!std::holds_alternative<std::monostate>(result)) {
			throw WinFutureError(WinFutureErrc::promise_already_satisfied);
		}
		result.template emplace<std::exception_ptr>(eptr);
		ready = true;
		condition.notify_all();
	}
};

template <typename T>
class WinFutureStateManager {
public:
	WinFutureStateManager() = default;
	WinFutureStateManager(std::shared_ptr<WinFutureState<T>> new_state)
	: state(std::move(new_state)) {}
	WinFutureStateManager(const WinFutureStateManager &) = default;
	WinFutureStateManager &operator=(const WinFutureStateManager &) = default;
	WinFutureStateManager(WinFutureStateManager &&) = default;
	WinFutureStateManager &operator=(WinFutureStateManager &&) = default;

	[[nodiscard]] bool valid() const noexcept { return static_cast<bool>(state); }

	void wait() const {
		if (!valid()) {
			throw WinFutureError(WinFutureErrc::no_state);
		}
		state->wait();
	}

	template <class Rep, class Period>
	WinFutureStatus
	wait_for(const std::chrono::duration<Rep, Period> &timeout_duration) const {
		if (!valid()) {
			throw WinFutureError(WinFutureErrc::no_state);
		}
		return state->wait_for(timeout_duration);
	}

	T &get_value() const {
		if (!valid()) {
			throw WinFutureError(WinFutureErrc::no_state);
		}
		return state->get_value();
	}

	void set_value(const T &value) {
		if (!valid()) {
			throw WinFutureError(WinFutureErrc::no_state);
		}
		state->set_value(value);
	}

	void set_value(T &&value) {
		if (!valid()) {
			throw WinFutureError(WinFutureErrc::no_state);
		}
		state->set_value(std::move(value));
	}

	void set_exception(std::exception_ptr eptr) {
		if (!valid()) {
			throw WinFutureError(WinFutureErrc::no_state);
		}
		state->set_exception(eptr);
	}

private:
	std::shared_ptr<WinFutureState<T>> state;
};

template <typename T>
class WinFuture : public WinFutureStateManager<T> {
	using Base = WinFutureStateManager<T>;
	static_assert(!std::is_array_v<T> && std::is_object_v<T> &&
			      std::is_destructible_v<T>,
		      "T in future<T> must meet the Cpp17Destructible requirements "
		      "(N4878 [futures.unique.future]/4).");

public:
	WinFuture() noexcept = default;
	WinFuture(WinFuture &&) noexcept = default;
	WinFuture &operator=(WinFuture &&) noexcept = default;
	WinFuture(const WinFuture &) noexcept = delete;
	WinFuture &operator=(const WinFuture &) noexcept = delete;

	WinFuture(const Base &base, std::monostate) : Base(base) {}
	~WinFuture() noexcept = default;
	T get() {
		WinFuture local(std::move(*this));
		return std::move(local.get_value());
	}

private:
	using Base::get_value;
	using Base::set_exception;
	using Base::set_value;
};

template <typename T>
class WinFuture<T &> : public WinFutureStateManager<T *> {
	using Base = WinFutureStateManager<T *>;

public:
	WinFuture() noexcept = default;
	WinFuture(WinFuture &&) noexcept = default;
	WinFuture &operator=(WinFuture &&) noexcept = default;
	WinFuture(const WinFuture &) noexcept = delete;
	WinFuture &operator=(const WinFuture &) noexcept = delete;

	WinFuture(const Base &base, std::monostate) : Base(base) {}
	~WinFuture() noexcept = default;
	T &get() {
		WinFuture local(std::move(*this));
		return *local.get_value();
	}

private:
	using Base::get_value;
	using Base::set_exception;
	using Base::set_value;
};

template <>
class WinFuture<void> : public WinFutureStateManager<int> {
	using Base = WinFutureStateManager<int>;

public:
	WinFuture() noexcept = default;
	WinFuture(WinFuture &&) noexcept = default;
	WinFuture &operator=(WinFuture &&) noexcept = default;
	WinFuture(const WinFuture &) noexcept = delete;
	WinFuture &operator=(const WinFuture &) noexcept = delete;

	WinFuture(const Base &base, std::monostate) : Base(base) {}
	~WinFuture() noexcept = default;
	void get() {
		WinFuture local(std::move(*this));
		local.get_value();
	}

private:
	using Base::get_value;
	using Base::set_exception;
	using Base::set_value;
};

template <typename T>
class WinPromiseBase {
public:
	WinPromiseBase(std::shared_ptr<WinFutureState<T>> new_state)
	: state(std::move(new_state)) {}
	WinPromiseBase(WinPromiseBase &&) = default;
	WinPromiseBase &operator=(WinPromiseBase &&) = default;
	WinPromiseBase(const WinPromiseBase &) = delete;
	WinPromiseBase &operator=(const WinPromiseBase &) = delete;

	WinFutureStateManager<T> &get_state_for_set() {
		if (!state.valid()) {
			throw WinFutureError(WinFutureErrc::no_state);
		}
		return state;
	}

	WinFutureStateManager<T> &get_state_for_future() {
		if (!state.valid()) {
			throw WinFutureError(WinFutureErrc::no_state);
		}
		if (future_retrieved) {
			throw WinFutureError(WinFutureErrc::future_already_retrieved);
		}
		future_retrieved = true;
		return state;
	}

private:
	WinFutureStateManager<T> state;
	bool future_retrieved = false;
};

template <typename T>
class WinPromise {
public:
	WinPromise() : base(std::make_shared<WinFutureState<T>>()) {}
	WinPromise(WinPromise &&) = default;
	WinPromise(const WinPromise &) = delete;
	~WinPromise() noexcept {}
	[[nodiscard]] WinFuture<T> get_future() {
		return WinFuture<T>(base.get_state_for_future(), std::monostate());
	}
	void set_value(const T &value) { base.get_state_for_set().set_value(value); }
	void set_value(T &&value) {
		base.get_state_for_set().set_value(std::forward<T>(value));
	}
	void set_exception(std::exception_ptr eptr) {
		base.get_state_for_set().set_exception(eptr);
	}

private:
	WinPromiseBase<T> base;
};

template <typename T>
class WinPromise<T &> {
public:
	WinPromise() : base(std::make_shared<WinFutureState<T *>>()) {}
	WinPromise(WinPromise &&) = default;
	WinPromise(const WinPromise &) = delete;
	~WinPromise() noexcept {}
	[[nodiscard]] WinFuture<T &> get_future() {
		return WinFuture<T>(base.get_state_for_future(), std::monostate());
	}
	void set_value(T &value) {
		base.get_state_for_set().set_value(std::addressof(value));
	}
	void set_exception(std::exception_ptr eptr) {
		base.get_state_for_set().set_exception(eptr);
	}

private:
	WinPromiseBase<T *> base;
};

template <>
class WinPromise<void> {
public:
	WinPromise() : base(std::make_shared<WinFutureState<int>>()) {}
	WinPromise(WinPromise &&) = default;
	WinPromise(const WinPromise &) = delete;
	~WinPromise() noexcept {}
	[[nodiscard]] WinFuture<void> get_future() {
		return WinFuture<void>(base.get_state_for_future(), std::monostate());
	}
	void set_value() { base.get_state_for_set().set_value(0); }
	void set_exception(std::exception_ptr eptr) {
		base.get_state_for_set().set_exception(eptr);
	}

private:
	WinPromiseBase<int> base;
};

#endif
