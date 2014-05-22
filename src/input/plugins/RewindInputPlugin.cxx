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
#include "RewindInputPlugin.hxx"
#include "../ProxyInputStream.hxx"

#include <assert.h>
#include <string.h>

class RewindInputStream final : public ProxyInputStream {
	/**
	 * The read position within the buffer.  Undefined as long as
	 * ReadingFromBuffer() returns false.
	 */
	size_t head;

	/**
	 * The write/append position within the buffer.
	 */
	size_t tail;

	/**
	 * The size of this buffer is the maximum number of bytes
	 * which can be rewinded cheaply without passing the "seek"
	 * call to CURL.
	 *
	 * The origin of this buffer is always the beginning of the
	 * stream (offset 0).
	 */
	char buffer[64 * 1024];

public:
	RewindInputStream(InputStream *_input)
		:ProxyInputStream(_input),
		 tail(0) {
	}

	/* virtual methods from InputStream */

	void Update() override {
		if (!ReadingFromBuffer())
			ProxyInputStream::Update();
	}

	bool IsEOF() override {
		return !ReadingFromBuffer() && ProxyInputStream::IsEOF();
	}

	size_t Read(void *ptr, size_t size, Error &error) override;
	bool Seek(offset_type offset, Error &error) override;

private:
	/**
	 * Are we currently reading from the buffer, and does the
	 * buffer contain more data for the next read operation?
	 */
	bool ReadingFromBuffer() const {
		return tail > 0 && offset < input.GetOffset();
	}
};

size_t
RewindInputStream::Read(void *ptr, size_t read_size, Error &error)
{
	if (ReadingFromBuffer()) {
		/* buffered read */

		assert(head == (size_t)offset);
		assert(tail == (size_t)input.GetOffset());

		if (read_size > tail - head)
			read_size = tail - head;

		memcpy(ptr, buffer + head, read_size);
		head += read_size;
		offset += read_size;

		return read_size;
	} else {
		/* pass method call to underlying stream */

		size_t nbytes = input.Read(ptr, read_size, error);

		if (input.GetOffset() > (offset_type)sizeof(buffer))
			/* disable buffering */
			tail = 0;
		else if (tail == (size_t)offset) {
			/* append to buffer */

			memcpy(buffer + tail, ptr, nbytes);
			tail += nbytes;

			assert(tail == (size_t)input.GetOffset());
		}

		CopyAttributes();

		return nbytes;
	}
}

bool
RewindInputStream::Seek(offset_type new_offset,
			Error &error)
{
	assert(IsReady());

	if (tail > 0 && new_offset <= (offset_type)tail) {
		/* buffered seek */

		assert(!ReadingFromBuffer() ||
		       head == (size_t)offset);
		assert(tail == (size_t)input.GetOffset());

		head = (size_t)new_offset;
		offset = new_offset;

		return true;
	} else {
		/* disable the buffer, because input has left the
		   buffered range now */
		tail = 0;

		return ProxyInputStream::Seek(new_offset, error);
	}
}

InputStream *
input_rewind_open(InputStream *is)
{
	assert(is != nullptr);
	assert(!is->IsReady() || is->GetOffset() == 0);

	if (is->IsReady() && is->IsSeekable())
		/* seekable resources don't need this plugin */
		return is;

	return new RewindInputStream(is);
}
