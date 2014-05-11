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

extern const InputPlugin rewind_input_plugin;

class RewindInputStream {
	InputStream base;

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
		:base(rewind_input_plugin, _input->GetURI(),
		      _input->mutex, _input->cond),
		 input(_input), tail(0) {
	}

	~RewindInputStream() {
		input->Close();
	}

	InputStream *GetBase() {
		return &base;
	}

	bool Check(Error &error) {
		return input->Check(error);
	}

	void Update() {
		if (!ReadingFromBuffer())
			CopyAttributes();
	}

	Tag *ReadTag() {
		return input->ReadTag();
	}

	bool IsAvailable() {
		return input->IsAvailable();
	}

	size_t Read(void *ptr, size_t size, Error &error);

	bool IsEOF() {
		return !ReadingFromBuffer() && input->IsEOF();
	}

	bool Seek(InputPlugin::offset_type offset, int whence, Error &error);

private:
	/**
	 * Are we currently reading from the buffer, and does the
	 * buffer contain more data for the next read operation?
	 */
	bool ReadingFromBuffer() const {
		return tail > 0 && base.offset < input->offset;
	}

	/**
	 * Copy public attributes from the underlying input stream to the
	 * "rewind" input stream.  This function is called when a method of
	 * the underlying stream has returned, which may have modified these
	 * attributes.
	 */
	void CopyAttributes() {
		InputStream *dest = &base;
		const InputStream *src = input;

		assert(dest != src);

		if (!dest->IsReady() && src->IsReady()) {
			if (src->HasMimeType())
				dest->SetMimeType(src->GetMimeType());

			dest->size = src->GetSize();
			dest->seekable = src->IsSeekable();
			dest->SetReady();
		}

		dest->offset = src->offset;
	}
};

static void
input_rewind_close(InputStream *is)
{
	RewindInputStream *r = (RewindInputStream *)is;

	delete r;
}

static bool
input_rewind_check(InputStream *is, Error &error)
{
	RewindInputStream *r = (RewindInputStream *)is;

	return r->Check(error);
}

static void
input_rewind_update(InputStream *is)
{
	RewindInputStream *r = (RewindInputStream *)is;

	r->Update();
}

static Tag *
input_rewind_tag(InputStream *is)
{
	RewindInputStream *r = (RewindInputStream *)is;

	return r->ReadTag();
}

static bool
input_rewind_available(InputStream *is)
{
	RewindInputStream *r = (RewindInputStream *)is;

	return r->IsAvailable();
}

inline size_t
RewindInputStream::Read(void *ptr, size_t size, Error &error)
{
	if (ReadingFromBuffer()) {
		/* buffered read */

		assert(head == (size_t)base.offset);
		assert(tail == (size_t)input->offset);

		if (size > tail - head)
			size = tail - head;

		memcpy(ptr, buffer + head, size);
		head += size;
		base.offset += size;

		return size;
	} else {
		/* pass method call to underlying stream */

		size_t nbytes = input->Read(ptr, size, error);

		if (input->offset > (InputPlugin::offset_type)sizeof(buffer))
			/* disable buffering */
			tail = 0;
		else if (tail == (size_t)base.offset) {
			/* append to buffer */

			memcpy(buffer + tail, ptr, nbytes);
			tail += nbytes;

			assert(tail == (size_t)input->offset);
		}

		CopyAttributes();

		return nbytes;
	}
}

static size_t
input_rewind_read(InputStream *is, void *ptr, size_t size,
		  Error &error)
{
	RewindInputStream *r = (RewindInputStream *)is;

	return r->Read(ptr, size, error);
}

static bool
input_rewind_eof(InputStream *is)
{
	RewindInputStream *r = (RewindInputStream *)is;

	return r->IsEOF();
}

inline bool
RewindInputStream::Seek(InputPlugin::offset_type offset, int whence,
			Error &error)
{
	assert(base.IsReady());

	if (whence == SEEK_SET && tail > 0 &&
	    offset <= (InputPlugin::offset_type)tail) {
		/* buffered seek */

		assert(!ReadingFromBuffer() ||
		       head == (size_t)base.offset);
		assert(tail == (size_t)input->offset);

		head = (size_t)offset;
		base.offset = offset;

		return true;
	} else {
		bool success = input->Seek(offset, whence, error);
		CopyAttributes();

		/* disable the buffer, because input has left the
		   buffered range now */
		tail = 0;

		return success;
	}
}

static bool
input_rewind_seek(InputStream *is, InputPlugin::offset_type offset,
		  int whence,
		  Error &error)
{
	RewindInputStream *r = (RewindInputStream *)is;

	return r->Seek(offset, whence, error);
}

const InputPlugin rewind_input_plugin = {
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	input_rewind_close,
	input_rewind_check,
	input_rewind_update,
	input_rewind_tag,
	input_rewind_available,
	input_rewind_read,
	input_rewind_eof,
	input_rewind_seek,
};

InputStream *
input_rewind_open(InputStream *is)
{
	assert(is != nullptr);
	assert(is->offset == 0);

	if (is->IsReady() && is->IsSeekable())
		/* seekable resources don't need this plugin */
		return is;

	RewindInputStream *c = new RewindInputStream(is);
	return c->GetBase();
}
