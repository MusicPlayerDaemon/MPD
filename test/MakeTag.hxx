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

#include "tag/Builder.hxx"
#include "tag/Tag.hxx"
#include "util/Compiler.h"

inline void
BuildTag([[maybe_unused]] TagBuilder &tag) noexcept
{
}

template<typename... Args>
inline void
BuildTag(TagBuilder &tag, TagType type, const char *value,
	 Args&&... args) noexcept
{
	tag.AddItem(type, value);
	BuildTag(tag, std::forward<Args>(args)...);
}

template<typename... Args>
inline Tag
MakeTag(Args&&... args) noexcept
{
	TagBuilder tag;
	BuildTag(tag, std::forward<Args>(args)...);
	return tag.Commit();
}
