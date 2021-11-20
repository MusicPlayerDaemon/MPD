/*
 * Copyright 2003-2021 The Music Player Daemon Project
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
#include "InputStream.hxx"
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
		const std::scoped_lock<Mutex> lock(mutex);
		stop = true;
		wake_cond.notify_one();
	}

	thread.Join();
}

void
BufferingInputStream::Check()
{
	if (error)
		std::rethrow_exception(error);

	if (input)
		input->Check();
}

bool
BufferingInputStream::IsAvailable(size_t offset) const noexcept
{
	if (offset >= size() || error)
		return true;

	if (buffer.Read(offset).HasData())
		return true;

	/* if no data is available now, make sure it will be soon */
	if (want_offset == INVALID_OFFSET)
		want_offset = offset;

	return false;
}

size_t
BufferingInputStream::Read(std::unique_lock<Mutex> &lock, size_t offset,
			   void *ptr, size_t s)
{
	if (offset >= size())
		return 0;

	while (true) {
		auto r = buffer.Read(offset);
		if (r.HasData()) {
			/* yay, we have some data */
			size_t nbytes = std::min(s, r.defined_buffer.size);
			memcpy(ptr, r.defined_buffer.data, nbytes);
			return nbytes;
		}

		if (error)
			std::rethrow_exception(error);

		if (want_offset == INVALID_OFFSET)
			want_offset = offset;

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
		if (want_offset != INVALID_OFFSET) {
			assert(want_offset < size());

			const size_t seek_offset = want_offset;
			want_offset = INVALID_OFFSET;
			if (!buffer.Read(seek_offset).HasData())
				input->Seek(lock, seek_offset);
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
				size_t new_offset = FindFirstHole();
				if (new_offset == INVALID_OFFSET)
					/* the file has been read
					   completely */
					break;

				input->Seek(lock, new_offset);

				continue;
			}

			/* enforce an upper limit for each
			   InputStream::Read() call; this is necessary
			   for plugins which are unable to do partial
			   reads, e.g. when reading local files, the
			   read() system call will not return until
			   all requested bytes have been read from the
			   hard disk, instead of returning when "some"
			   data has been read */
			constexpr size_t MAX_READ = 64 * 1024;
			if (w.size > MAX_READ)
				w.size = MAX_READ;

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
		error = std::current_exception();
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
