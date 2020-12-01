/*
 * Copyright 2020 The Music Player Daemon Project
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
#include "ComWorker.hxx"
#include "Log.hxx"
#include "thread/Name.hxx"
#include "util/Domain.hxx"
#include "win32/Com.hxx"

namespace {
static constexpr Domain com_worker_domain("com_worker");
}

std::mutex COMWorker::mutex;
unsigned int COMWorker::reference_count = 0;
std::optional<COMWorker::COMWorkerThread> COMWorker::thread;

void COMWorker::COMWorkerThread::Work() noexcept {
	FormatDebug(com_worker_domain, "Working thread started");
	SetThreadName("COM Worker");
	COM com{true};
	while (true) {
		if (!running_flag.test_and_set()) {
			FormatDebug(com_worker_domain, "Working thread ended");
			return;
		}
		while (!spsc_buffer.empty()) {
			std::function<void()> function;
			spsc_buffer.pop(function);
			function();
		}
		event.Wait(200);
	}
}
