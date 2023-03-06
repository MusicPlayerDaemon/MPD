// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#undef NOUSER // COM needs the "MSG" typedef

#include "ComWorker.hxx"
#include "Com.hxx"
#include "thread/Name.hxx"

void
COMWorker::Work() noexcept
{
	SetThreadName("COM Worker");
	COM com;

	std::unique_lock lock{mutex};
	while (running_flag) {
		while (!queue.empty()) {
			auto function = std::move(queue.front());
			queue.pop();

			lock.unlock();
			function();
			lock.lock();
		}

		cond.wait(lock);
	}
}
