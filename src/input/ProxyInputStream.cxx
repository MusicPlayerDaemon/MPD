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
#include "ProxyInputStream.hxx"
#include "tag/Tag.hxx"

#include <assert.h>

ProxyInputStream::ProxyInputStream(InputStream *_input)
	:InputStream(_input->GetURI(), _input->mutex, _input->cond),
	 input(*_input) {}

ProxyInputStream::~ProxyInputStream()
{
	delete &input;
}

void
ProxyInputStream::CopyAttributes()
{
	if (input.IsReady()) {
		if (!IsReady()) {
			if (input.HasMimeType())
				SetMimeType(input.GetMimeType());

			size = input.KnownSize()
				? input.GetSize()
				: UNKNOWN_SIZE;

			seekable = input.IsSeekable();
			SetReady();
		}

		offset = input.GetOffset();
	}
}

bool
ProxyInputStream::Check(Error &error)
{
	return input.Check(error);
}

void
ProxyInputStream::Update()
{
	input.Update();
	CopyAttributes();
}

bool
ProxyInputStream::Seek(offset_type new_offset, Error &error)
{
	bool success = input.Seek(new_offset, error);
	CopyAttributes();
	return success;
}

bool
ProxyInputStream::IsEOF()
{
	return input.IsEOF();
}

Tag *
ProxyInputStream::ReadTag()
{
	return input.ReadTag();
}

bool
ProxyInputStream::IsAvailable()
{
	return input.IsAvailable();
}

size_t
ProxyInputStream::Read(void *ptr, size_t read_size, Error &error)
{
	size_t nbytes = input.Read(ptr, read_size, error);
	CopyAttributes();
	return nbytes;
}
