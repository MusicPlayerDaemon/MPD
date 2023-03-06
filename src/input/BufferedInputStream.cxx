// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "BufferedInputStream.hxx"

#include <string.h>

BufferedInputStream::BufferedInputStream(InputStreamPtr _input)
	:InputStream(_input->GetUriView(), _input->mutex),
	 BufferingInputStream(std::move(_input))
{
	assert(IsEligible(GetInput()));

	if (GetInput().HasMimeType())
		SetMimeType(GetInput().GetMimeType());

	InputStream::size = BufferingInputStream::size();
	InputStream::seekable = GetInput().IsSeekable();
	InputStream::offset = GetInput().GetOffset();

	SetReady();
}

void
BufferedInputStream::Check()
{
	BufferingInputStream::Check();
}

void
BufferedInputStream::Seek(std::unique_lock<Mutex> &,
			  offset_type new_offset)
{
	offset = new_offset;
}

bool
BufferedInputStream::IsEOF() const noexcept
{
	return InputStream::offset == BufferingInputStream::size();
}

bool
BufferedInputStream::IsAvailable() const noexcept
{
	return BufferingInputStream::IsAvailable(offset);
}

size_t
BufferedInputStream::Read(std::unique_lock<Mutex> &lock,
			  void *ptr, size_t s)
{
	size_t nbytes = BufferingInputStream::Read(lock, offset, ptr, s);
	InputStream::offset += nbytes;
	return nbytes;
}
