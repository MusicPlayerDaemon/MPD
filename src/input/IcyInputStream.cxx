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
#include "IcyInputStream.hxx"
#include "tag/Tag.hxx"

IcyInputStream::IcyInputStream(InputStream *_input)
	:ProxyInputStream(_input),
	 input_tag(nullptr), icy_tag(nullptr),
	 override_offset(0)
{
}

IcyInputStream::~IcyInputStream()
{
	delete input_tag;
	delete icy_tag;
}

void
IcyInputStream::Update()
{
	ProxyInputStream::Update();

	if (IsEnabled())
		offset = override_offset;
}

Tag *
IcyInputStream::ReadTag()
{
	Tag *new_input_tag = ProxyInputStream::ReadTag();
	if (!IsEnabled())
		return new_input_tag;

	if (new_input_tag != nullptr) {
		delete input_tag;
		input_tag = new_input_tag;
	}

	Tag *new_icy_tag = parser.ReadTag();
	if (new_icy_tag != nullptr) {
		delete icy_tag;
		icy_tag = new_icy_tag;
	}

	if (new_input_tag == nullptr && new_icy_tag == nullptr)
		/* no change */
		return nullptr;

	if (input_tag == nullptr && icy_tag == nullptr)
		/* no tag */
		return nullptr;

	if (input_tag == nullptr)
		return new Tag(*icy_tag);

	if (icy_tag == nullptr)
		return new Tag(*input_tag);

	return Tag::Merge(*input_tag, *icy_tag);
}

size_t
IcyInputStream::Read(void *ptr, size_t read_size, Error &error)
{
	if (!IsEnabled())
		return ProxyInputStream::Read(ptr, read_size, error);

	while (true) {
		size_t nbytes = ProxyInputStream::Read(ptr, read_size, error);
		if (nbytes == 0)
			return 0;

		size_t result = parser.ParseInPlace(ptr, nbytes);
		if (result > 0) {
			override_offset += result;
			offset = override_offset;
			return result;
		}
	}
}
