/*
 * Copyright 2003-2017 The Music Player Daemon Project
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

#include "config.h"
#include "ThreadInputStream.hxx"
#include "thread/Name.hxx"
#include "util/CircularBuffer.hxx"
#include "util/HugeAllocator.hxx"

#include <assert.h>
#include <string.h>

ThreadInputStream::~ThreadInputStream()
{
	{
		const std::lock_guard<Mutex> lock(mutex);
		close = true;
		wake_cond.signal();
	}

	Cancel();

	thread.Join();

	if (buffer != nullptr) {
		buffer->Clear();
		HugeFree(buffer->Write().data, buffer_size);
		delete buffer;
	}
}

void
ThreadInputStream::Start()
{
	assert(buffer == nullptr);

	void *p = HugeAllocate(buffer_size);
	assert(p != nullptr);

	buffer = new CircularBuffer<uint8_t>((uint8_t *)p, buffer_size);
	thread.Start();
}

void
ThreadInputStream::ThreadFunc()
{
	FormatThreadName("input:%s", plugin);

	const std::lock_guard<Mutex> lock(mutex);

	try {
		Open();
	} catch (...) {
		postponed_exception = std::current_exception();
		SetReady();
		return;
	}

	/* we're ready, tell it to our client */
	SetReady();

	while (!close) {
		assert(!postponed_exception);

		auto w = buffer->Write();
		if (w.IsEmpty()) {
			wake_cond.wait(mutex);
		} else {
			size_t nbytes;

			try {
				const ScopeUnlock unlock(mutex);
				nbytes = ThreadRead(w.data, w.size);
			} catch (...) {
				postponed_exception = std::current_exception();
				cond.broadcast();
				break;
			}

			cond.broadcast();

			if (nbytes == 0) {
				eof = true;
				break;
			}

			buffer->Append(nbytes);
		}
	}

	Close();
}

void
ThreadInputStream::Check()
{
	assert(!thread.IsInside());

	if (postponed_exception)
		std::rethrow_exception(postponed_exception);
}

bool
ThreadInputStream::IsAvailable() noexcept
{
	assert(!thread.IsInside());

	return !buffer->IsEmpty() || eof || postponed_exception;
}

inline size_t
ThreadInputStream::Read(void *ptr, size_t read_size)
{
	assert(!thread.IsInside());

	while (true) {
		if (postponed_exception)
			std::rethrow_exception(postponed_exception);

		auto r = buffer->Read();
		if (!r.IsEmpty()) {
			size_t nbytes = std::min(read_size, r.size);
			memcpy(ptr, r.data, nbytes);
			buffer->Consume(nbytes);
			wake_cond.broadcast();
			offset += nbytes;
			return nbytes;
		}

		if (eof)
			return 0;

		cond.wait(mutex);
	}
}

bool
ThreadInputStream::IsEOF() noexcept
{
	assert(!thread.IsInside());

	return eof;
}
