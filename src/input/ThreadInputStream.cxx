/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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
#include "InputPlugin.hxx"
#include "thread/Name.hxx"
#include "util/CircularBuffer.hxx"
#include "util/HugeAllocator.hxx"

#include <assert.h>
#include <string.h>

ThreadInputStream::~ThreadInputStream()
{
	if (buffer != nullptr) {
		buffer->Clear();
		HugeFree(buffer->Write().data, buffer_size);
		delete buffer;
	}
}

InputStream *
ThreadInputStream::Start(Error &error)
{
	assert(buffer == nullptr);

	void *p = HugeAllocate(buffer_size);
	if (p == nullptr) {
		error.SetErrno();
		return nullptr;
	}

	buffer = new CircularBuffer<uint8_t>((uint8_t *)p, buffer_size);

	if (!thread.Start(ThreadFunc, this, error))
		return nullptr;

	return &base;
}

inline void
ThreadInputStream::ThreadFunc()
{
	FormatThreadName("input:%s", base.GetPlugin().name);

	Lock();
	if (!Open(postponed_error)) {
		base.cond.broadcast();
		Unlock();
		return;
	}

	/* we're ready, tell it to our client */
	base.SetReady();

	while (!close) {
		assert(!postponed_error.IsDefined());

		auto w = buffer->Write();
		if (w.IsEmpty()) {
			wake_cond.wait(base.mutex);
		} else {
			Unlock();

			Error error;
			size_t nbytes = Read(w.data, w.size, error);

			Lock();
			base.cond.broadcast();

			if (nbytes == 0) {
				eof = true;
				postponed_error = std::move(error);
				break;
			}

			buffer->Append(nbytes);
		}
	}

	Unlock();

	Close();
}

void
ThreadInputStream::ThreadFunc(void *ctx)
{
	ThreadInputStream &tis = *(ThreadInputStream *)ctx;
	tis.ThreadFunc();
}

inline bool
ThreadInputStream::Check2(Error &error)
{
	if (postponed_error.IsDefined()) {
		error = std::move(postponed_error);
		return false;
	}

	return true;
}

bool
ThreadInputStream::Check(InputStream *is, Error &error)
{
	return Cast(is)->Check2(error);
}

inline bool
ThreadInputStream::Available2()
{
	return !buffer->IsEmpty() || eof || postponed_error.IsDefined();
}

bool
ThreadInputStream::Available(InputStream *is)
{
	return Cast(is)->Available2();
}

inline size_t
ThreadInputStream::Read2(void *ptr, size_t size, Error &error)
{
	while (true) {
		if (postponed_error.IsDefined()) {
			error = std::move(postponed_error);
			return 0;
		}

		auto r = buffer->Read();
		if (!r.IsEmpty()) {
			size_t nbytes = std::min(size, r.size);
			memcpy(ptr, r.data, nbytes);
			buffer->Consume(nbytes);
			wake_cond.broadcast();
			base.offset += nbytes;
			return nbytes;
		}

		if (eof)
			return 0;

		base.cond.wait(base.mutex);
	}
}

size_t
ThreadInputStream::Read(InputStream *is, void *ptr, size_t size,
			Error &error)
{
	return Cast(is)->Read2(ptr, size, error);
}

inline void
ThreadInputStream::Close2()
{
	Lock();
	close = true;
	wake_cond.signal();
	Unlock();

	Cancel();

	thread.Join();

	delete this;
}

void
ThreadInputStream::Close(InputStream *is)
{
	Cast(is)->Close2();
}

inline bool
ThreadInputStream::IsEOF2()
{
	return eof;
}

bool
ThreadInputStream::IsEOF(InputStream *is)
{
	return Cast(is)->IsEOF2();
}
