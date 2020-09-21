/*
 * Copyright 2003-2020 The Music Player Daemon Project
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

#include "TaggedInputStream.hxx"
#include "tag/Tag.hxx"

TaggedInputStream::TaggedInputStream(InputStreamPtr _input, std::unique_ptr<Tag> &&_tag) :
	ProxyInputStream(std::move(_input)), tag(std::move(_tag))
{}

std::unique_ptr<Tag> TaggedInputStream::ReadTag() noexcept {
	auto inner_tag = ProxyInputStream::ReadTag();

	if(!inner_tag)
		return std::exchange(tag, nullptr);
	else if(!tag)
		return inner_tag;
	else
		return Tag::Merge(std::exchange(tag, nullptr), std::move(inner_tag));
}
