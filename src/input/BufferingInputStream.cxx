/*
 * Copyright 2003-2019 The Music Player Daemon Project
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

#include "BufferingInputStream.hxx"
#include "thread/Cond.hxx"
#include "thread/Name.hxx"

#include <string.h>

BufferingInputStream::BufferingInputStream(InputStreamPtr _input)
	:input(std::move(_input)),
	 mutex(input->mutex),
	 thread(BIND_THIS_METHOD(RunThread)),
	 buffer(input->GetSize())
{
	input->SetHandler(this);

	thread.Start();
}

BufferingInputStream::~BufferingInputStream() noexcept
{
	{
		const std::lock_guard<Mutex> lock(mutex);
		stop = true;
		wake_cond.notify_all();
	}

	thread.Join();
}

void
BufferingInputStream::Check()
{
	if (read_error)
		std::rethrow_exception(read_error);

	if (input)
		input->Check();
}

void
BufferingInputStream::Seek(std::unique_lock<Mutex> &lock, size_t new_offset)
{
	if (new_offset >= size()) {
		offset = new_offset;
		return;
	}

	auto r = buffer.Read(new_offset);
	if (r.HasData()) {
		/* nice, we already have some data at the desired
		   offset and this method call is a no-op */
		offset = new_offset;
		return;
	}

	seek_offset = new_offset;
	seek = true;
	wake_cond.notify_all();

	client_cond.wait(lock, [this]{ return !seek; });

	if (seek_error)
		std::rethrow_exception(std::exchange(seek_error, {}));

	offset = new_offset;
}

bool
BufferingInputStream::IsAvailable() noexcept
{
	return offset == size() || buffer.Read(offset).HasData();
}

size_t
BufferingInputStream::Read(std::unique_lock<Mutex> &lock, void *ptr, size_t s)
{
	if (offset >= size())
		return 0;

	while (true) {
		auto r = buffer.Read(offset);
		if (r.HasData()) {
			/* yay, we have some data */
			size_t nbytes = std::min(s, r.defined_buffer.size);
			memcpy(ptr, r.defined_buffer.data, nbytes);
			offset += nbytes;

			if (!IsAvailable()) {
				/* wake up the sleeping thread */
				wake_cond.notify_all();
			}

			return nbytes;
		}

		if (read_error)
			std::rethrow_exception(read_error);

		client_cond.wait(lock);
	}
}

size_t
BufferingInputStream::FindFirstHole() const noexcept
{
	auto r = buffer.Read(0);
	if (r.undefined_size > 0)
		/* a hole at the beginning */
		return 0;

	if (r.defined_buffer.size < size())
		/* a hole in the middle */
		return r.defined_buffer.size;

	/* the file has been read completely */
	return INVALID_OFFSET;
}

inline void
BufferingInputStream::RunThreadLocked(std::unique_lock<Mutex> &lock)
{
	while (!stop) {
		if (seek) {
			try {
				input->Seek(lock, seek_offset);
			} catch (...) {
				seek_error = std::current_exception();
			}

			seek = false;
			client_cond.notify_all();
		} else if (offset != input->GetOffset() && !IsAvailable()) {
			/* a past Seek() call was a no-op because data
			   was already available at that position, but
			   now we've reached a new position where
			   there is no more data in the buffer, and
			   our input is reading somewhere else (maybe
			   stuck at the end of the file); to find a
			   way out, we now seek our input to our
			   reading position to be able to fill our
			   buffer */

			input->Seek(lock, offset);
		} else if (input->IsEOF()) {
			/* our input has reached its end: prepare
			   reading the first remaining hole */

			size_t new_offset = FindFirstHole();
			if (new_offset == INVALID_OFFSET) {
				/* the file has been read completely */
				break;
			}

			/* seek to the first hole */
			input->Seek(lock, new_offset);
		} else if (input->IsAvailable()) {
			const auto read_offset = input->GetOffset();
			auto w = buffer.Write(read_offset);

			if (w.empty()) {
				if (IsAvailable()) {
					/* we still have enough data
					   for the next Read() - seek
					   to the first hole */

					size_t new_offset = FindFirstHole();
					if (new_offset == INVALID_OFFSET)
						/* the file has been
						   read completely */
						break;

					input->Seek(lock, new_offset);
				} else {
					/* we need more data at our
					   current position, because
					   the next Read() will stall
					   - seek our input to our
					   offset to prepare filling
					   the buffer from there */
					input->Seek(lock, offset);
				}

				continue;
			}

			size_t nbytes = input->Read(lock, w.data, w.size);
			buffer.Commit(read_offset, read_offset + nbytes);

			client_cond.notify_all();
			OnBufferAvailable();
		} else
			wake_cond.wait(lock);
	}
}

void
BufferingInputStream::RunThread() noexcept
{
	SetThreadName("buffering");

	std::unique_lock<Mutex> lock(mutex);

	try {
		RunThreadLocked(lock);
	} catch (...) {
		read_error = std::current_exception();
		client_cond.notify_all();
		OnBufferAvailable();
	}

	/* clear the "input" attribute while holding the mutex */
	auto _input = std::move(input);

	/* the mutex must be unlocked while an InputStream can be
	   destructed */
	lock.unlock();

	/* and now actually destruct the InputStream */
	_input.reset();
}
