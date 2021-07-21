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

#ifndef MPD_WIN32_COM_WORKER_HXX
#define MPD_WIN32_COM_WORKER_HXX

#include "WinEvent.hxx"
#include "thread/Future.hxx"
#include "thread/Thread.hxx"

#include <boost/lockfree/spsc_queue.hpp>

// Worker thread for all COM operation
class COMWorker {
	Thread thread{BIND_THIS_METHOD(Work)};

	boost::lockfree::spsc_queue<std::function<void()>> spsc_buffer{32};
	std::atomic_flag running_flag = true;
	WinEvent event{};

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
		Push([function = std::forward<Function>(function),
			      promise = std::move(promise)]() mutable {
			try {
				if constexpr (std::is_void_v<R>) {
					std::invoke(std::forward<Function>(function));
					promise->set_value();
				} else {
					promise->set_value(std::invoke(std::forward<Function>(function)));
				}
			} catch (...) {
				promise->set_exception(std::current_exception());
			}
		});
		return future;
	}

private:
	void Finish() noexcept {
		running_flag.clear();
		event.Set();
	}

	void Push(const std::function<void()> &function) {
		spsc_buffer.push(function);
		event.Set();
	}

	void Work() noexcept;
};

#endif
