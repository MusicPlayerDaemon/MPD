// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_WIN32_COM_WORKER_HXX
#define MPD_WIN32_COM_WORKER_HXX

#include "thread/Cond.hxx"
#include "thread/Future.hxx"
#include "thread/Mutex.hxx"
#include "thread/Thread.hxx"

#include <functional>
#include <queue>

// Worker thread for all COM operation
class COMWorker {
	Mutex mutex;
	Cond cond;

	std::queue<std::function<void()>> queue;
	bool running_flag = true;

	Thread thread{BIND_THIS_METHOD(Work)};

public:
	COMWorker() {
		thread.Start();
	}

	~COMWorker() noexcept {
		Finish();
		thread.Join();
	}

	COMWorker(const COMWorker &) = delete;
	COMWorker &operator=(const COMWorker &) = delete;

	template<typename Function>
	auto Async(Function &&function) {
		using R = std::invoke_result_t<std::decay_t<Function>>;
		auto promise = std::make_shared<Promise<R>>();
		auto future = promise->get_future();
		Push([func = std::forward<Function>(function),
			      prom = std::move(promise)]() mutable {
			try {
				if constexpr (std::is_void_v<R>) {
					std::invoke(std::forward<Function>(func));
					prom->set_value();
				} else {
					prom->set_value(std::invoke(std::forward<Function>(func)));
				}
			} catch (...) {
				prom->set_exception(std::current_exception());
			}
		});
		return future;
	}

private:
	void Finish() noexcept {
		const std::scoped_lock lock{mutex};
		running_flag = false;
		cond.notify_one();
	}

	void Push(const std::function<void()> &function) {
		const std::scoped_lock lock{mutex};
		queue.push(function);
		cond.notify_one();
	}

	void Work() noexcept;
};

#endif
