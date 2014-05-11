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
#include "../InputStream.hxx"
#include "../InputPlugin.hxx"

#include <assert.h>
#include <string.h>
#include <stdio.h>

class RewindInputStream final : public InputStream {
	InputStream *input;

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
		:InputStream(_input->GetURI(),
			     _input->mutex, _input->cond),
		 input(_input), tail(0) {
	}

	~RewindInputStream() {
		delete input;
	}

	/* virtual methods from InputStream */

	bool Check(Error &error) override {
		return input->Check(error);
	}

	void Update() override {
		if (!ReadingFromBuffer())
			CopyAttributes();
	}

	bool IsEOF() override {
		return !ReadingFromBuffer() && input->IsEOF();
	}

	Tag *ReadTag() override {
		return input->ReadTag();
	}

	bool IsAvailable() override {
		return input->IsAvailable();
	}

	size_t Read(void *ptr, size_t size, Error &error) override;
	bool Seek(offset_type offset, int whence, Error &error) override;

private:
	/**
	 * Are we currently reading from the buffer, and does the
	 * buffer contain more data for the next read operation?
	 */
	bool ReadingFromBuffer() const {
		return tail > 0 && offset < input->offset;
	}

	/**
	 * Copy public attributes from the underlying input stream to the
	 * "rewind" input stream.  This function is called when a method of
	 * the underlying stream has returned, which may have modified these
	 * attributes.
	 */
	void CopyAttributes() {
		const InputStream *src = input;

		assert(src != this);

		if (!IsReady() && src->IsReady()) {
			if (src->HasMimeType())
				SetMimeType(src->GetMimeType());

			size = src->GetSize();
			seekable = src->IsSeekable();
			SetReady();
		}

		offset = src->offset;
	}
};

size_t
RewindInputStream::Read(void *ptr, size_t read_size, Error &error)
{
	if (ReadingFromBuffer()) {
		/* buffered read */

		assert(head == (size_t)offset);
		assert(tail == (size_t)input->offset);

		if (read_size > tail - head)
			read_size = tail - head;

		memcpy(ptr, buffer + head, read_size);
		head += read_size;
		offset += read_size;

		return read_size;
	} else {
		/* pass method call to underlying stream */

		size_t nbytes = input->Read(ptr, read_size, error);

		if (input->offset > (InputPlugin::offset_type)sizeof(buffer))
			/* disable buffering */
			tail = 0;
		else if (tail == (size_t)offset) {
			/* append to buffer */

			memcpy(buffer + tail, ptr, nbytes);
			tail += nbytes;

			assert(tail == (size_t)input->offset);
		}

		CopyAttributes();

		return nbytes;
	}
}

inline bool
RewindInputStream::Seek(InputPlugin::offset_type new_offset, int whence,
			Error &error)
{
	assert(IsReady());

	if (whence == SEEK_SET && tail > 0 &&
	    new_offset <= (InputPlugin::offset_type)tail) {
		/* buffered seek */

		assert(!ReadingFromBuffer() ||
		       head == (size_t)offset);
		assert(tail == (size_t)input->offset);

		head = (size_t)new_offset;
		offset = new_offset;

		return true;
	} else {
		bool success = input->Seek(new_offset, whence, error);
		CopyAttributes();

		/* disable the buffer, because input has left the
		   buffered range now */
		tail = 0;

		return success;
	}
}

InputStream *
input_rewind_open(InputStream *is)
{
	assert(is != nullptr);
	assert(is->offset == 0);

	if (is->IsReady() && is->IsSeekable())
		/* seekable resources don't need this plugin */
		return is;

	return new RewindInputStream(is);
}
