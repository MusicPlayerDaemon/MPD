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

#include "BufferedInputStream.hxx"

#include <string.h>

BufferedInputStream::BufferedInputStream(InputStreamPtr _input)
	:InputStream(_input->GetURI(), _input->mutex),
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
