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
#include "Traits.hxx"

#include <string.h>

template<typename Traits>
typename Traits::string
BuildPathImpl(typename Traits::const_pointer a, size_t a_size,
	      typename Traits::const_pointer b, size_t b_size)
{
	assert(a != nullptr);
	assert(b != nullptr);

	if (a_size == 0)
		return typename Traits::string(b, b_size);
	if (b_size == 0)
		return typename Traits::string(a, a_size);

	typename Traits::string result(a, a_size);

	if (!Traits::IsSeparator(a[a_size - 1]))
		result.push_back(Traits::SEPARATOR);

	if (Traits::IsSeparator(b[0]))
		result.append(b + 1, b_size - 1);
	else
		result.append(b, b_size);

	return result;
}

template<typename Traits>
typename Traits::const_pointer
GetBasePathImpl(typename Traits::const_pointer p)
{
#if !CLANG_CHECK_VERSION(3,6)
	/* disabled on clang due to -Wtautological-pointer-compare */
	assert(p != nullptr);
#endif

	typename Traits::const_pointer sep = Traits::FindLastSeparator(p);
	return sep != nullptr
		? sep + 1
		: p;
}

template<typename Traits>
typename Traits::string
GetParentPathImpl(typename Traits::const_pointer p)
{
#if !CLANG_CHECK_VERSION(3,6)
	/* disabled on clang due to -Wtautological-pointer-compare */
	assert(p != nullptr);
#endif

	typename Traits::const_pointer sep = Traits::FindLastSeparator(p);
	if (sep == nullptr)
		return typename Traits::string(".");
	if (sep == p)
		return typename Traits::string(p, p + 1);
#ifdef WIN32
	if (Traits::IsDrive(p) && sep == p + 2)
		return typename Traits::string(p, p + 3);
#endif
	return typename Traits::string(p, sep);
}

template<typename Traits>
typename Traits::const_pointer
RelativePathImpl(typename Traits::const_pointer base,
		 typename Traits::const_pointer other)
{
	assert(base != nullptr);
	assert(other != nullptr);

	const auto base_length = Traits::GetLength(base);
	if (memcmp(base, other, base_length * sizeof(*base)) != 0)
		/* mismatch */
		return nullptr;

	other += base_length;
	if (other != 0) {
		if (!Traits::IsSeparator(*other))
			/* mismatch */
			return nullptr;

		/* skip remaining path separators */
		do {
			++other;
		} while (Traits::IsSeparator(*other));
	}

	return other;
}

PathTraitsFS::string
PathTraitsFS::Build(PathTraitsFS::const_pointer a, size_t a_size,
		    PathTraitsFS::const_pointer b, size_t b_size)
{
	return BuildPathImpl<PathTraitsFS>(a, a_size, b, b_size);
}

PathTraitsFS::const_pointer
PathTraitsFS::GetBase(PathTraitsFS::const_pointer p)
{
	return GetBasePathImpl<PathTraitsFS>(p);
}

PathTraitsFS::string
PathTraitsFS::GetParent(PathTraitsFS::const_pointer p)
{
	return GetParentPathImpl<PathTraitsFS>(p);
}

PathTraitsFS::const_pointer
PathTraitsFS::Relative(const_pointer base, const_pointer other)
{
	return RelativePathImpl<PathTraitsFS>(base, other);
}

PathTraitsUTF8::string
PathTraitsUTF8::Build(PathTraitsUTF8::const_pointer a, size_t a_size,
		      PathTraitsUTF8::const_pointer b, size_t b_size)
{
	return BuildPathImpl<PathTraitsUTF8>(a, a_size, b, b_size);
}

PathTraitsUTF8::const_pointer
PathTraitsUTF8::GetBase(PathTraitsUTF8::const_pointer p)
{
	return GetBasePathImpl<PathTraitsUTF8>(p);
}

PathTraitsUTF8::string
PathTraitsUTF8::GetParent(PathTraitsUTF8::const_pointer p)
{
	return GetParentPathImpl<PathTraitsUTF8>(p);
}

PathTraitsUTF8::const_pointer
PathTraitsUTF8::Relative(const_pointer base, const_pointer other)
{
	return RelativePathImpl<PathTraitsUTF8>(base, other);
}
