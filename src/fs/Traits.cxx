/*
 * Copyright 2003-2019 The Music Player Daemon Project
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

#include "Traits.hxx"
#include "util/StringCompare.hxx"

#include <string.h>

template<typename Traits>
typename Traits::string
BuildPathImpl(typename Traits::const_pointer_type a, size_t a_size,
	      typename Traits::const_pointer_type b, size_t b_size) noexcept
{
	assert(a != nullptr);
	assert(b != nullptr);

	if (a_size == 0)
		return typename Traits::string(b, b_size);
	if (b_size == 0)
		return typename Traits::string(a, a_size);

	typename Traits::string result;
	result.reserve(a_size + 1 + b_size);

	result.append(a, a_size);

	if (!Traits::IsSeparator(a[a_size - 1]))
		result.push_back(Traits::SEPARATOR);

	if (Traits::IsSeparator(b[0]))
		result.append(b + 1, b_size - 1);
	else
		result.append(b, b_size);

	return result;
}

template<typename Traits>
typename Traits::const_pointer_type
GetBasePathImpl(typename Traits::const_pointer_type p) noexcept
{
#if !CLANG_CHECK_VERSION(3,6)
	/* disabled on clang due to -Wtautological-pointer-compare */
	assert(p != nullptr);
#endif

	typename Traits::const_pointer_type sep = Traits::FindLastSeparator(p);
	return sep != nullptr
		? sep + 1
		: p;
}

template<typename Traits>
typename Traits::string
GetParentPathImpl(typename Traits::const_pointer_type p) noexcept
{
#if !CLANG_CHECK_VERSION(3,6)
	/* disabled on clang due to -Wtautological-pointer-compare */
	assert(p != nullptr);
#endif

	typename Traits::const_pointer_type sep = Traits::FindLastSeparator(p);
	if (sep == nullptr)
		return typename Traits::string(Traits::CURRENT_DIRECTORY);
	if (sep == p)
		return typename Traits::string(p, p + 1);
#ifdef _WIN32
	if (Traits::IsDrive(p) && sep == p + 2)
		return typename Traits::string(p, p + 3);
#endif
	return typename Traits::string(p, sep);
}

template<typename Traits>
typename Traits::const_pointer_type
RelativePathImpl(typename Traits::const_pointer_type base,
		 typename Traits::const_pointer_type other) noexcept
{
	assert(base != nullptr);
	assert(other != nullptr);

	other = StringAfterPrefix(other, base);
	if (other == nullptr)
		/* mismatch */
		return nullptr;

	if (*other != 0) {
		if (!Traits::IsSeparator(*other)) {
			if (*base != 0 && Traits::IsSeparator(other[-1]))
				/* "other" has no more slash, but the
				   matching base ended with a slash:
				   enough to detect a match */
				return other;

			/* mismatch */
			return nullptr;
		}

		/* skip remaining path separators */
		do {
			++other;
		} while (Traits::IsSeparator(*other));
	}

	return other;
}

PathTraitsFS::string
PathTraitsFS::Build(const_pointer_type a, size_t a_size,
		    const_pointer_type b, size_t b_size) noexcept
{
	return BuildPathImpl<PathTraitsFS>(a, a_size, b, b_size);
}

PathTraitsFS::const_pointer_type
PathTraitsFS::GetBase(PathTraitsFS::const_pointer_type p) noexcept
{
	return GetBasePathImpl<PathTraitsFS>(p);
}

PathTraitsFS::string
PathTraitsFS::GetParent(PathTraitsFS::const_pointer_type p) noexcept
{
	return GetParentPathImpl<PathTraitsFS>(p);
}

PathTraitsFS::const_pointer_type
PathTraitsFS::Relative(const_pointer_type base, const_pointer_type other) noexcept
{
	return RelativePathImpl<PathTraitsFS>(base, other);
}

PathTraitsFS::string
PathTraitsFS::Apply(const_pointer_type base, const_pointer_type path) noexcept
{
	// TODO: support the Windows syntax (absolute path with or without drive, drive with relative path)

	if (base == nullptr)
		return path;

	if (IsAbsolute(path))
		return path;

	return Build(base, path);
}

PathTraitsUTF8::string
PathTraitsUTF8::Build(const_pointer_type a, size_t a_size,
		      const_pointer_type b, size_t b_size) noexcept
{
	return BuildPathImpl<PathTraitsUTF8>(a, a_size, b, b_size);
}

PathTraitsUTF8::const_pointer_type
PathTraitsUTF8::GetBase(const_pointer_type p) noexcept
{
	return GetBasePathImpl<PathTraitsUTF8>(p);
}

PathTraitsUTF8::string
PathTraitsUTF8::GetParent(const_pointer_type p) noexcept
{
	return GetParentPathImpl<PathTraitsUTF8>(p);
}

PathTraitsUTF8::const_pointer_type
PathTraitsUTF8::Relative(const_pointer_type base,
			 const_pointer_type other) noexcept
{
	return RelativePathImpl<PathTraitsUTF8>(base, other);
}
