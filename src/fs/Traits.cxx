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

#include "Traits.hxx"
#include "util/StringCompare.hxx"
#include "util/UriExtract.hxx"

#include <string.h>

template<typename Traits>
typename Traits::string
BuildPathImpl(typename Traits::string_view a,
	      typename Traits::string_view b) noexcept
{
	if (a.empty())
		return typename Traits::string(b);
	if (b.empty())
		return typename Traits::string(a);

	typename Traits::string result;
	result.reserve(a.length() + 1 + b.length());

	result.append(a);

	if (!Traits::IsSeparator(a.back()))
		result.push_back(Traits::SEPARATOR);

	if (Traits::IsSeparator(b.front()))
		result.append(b.substr(1));
	else
		result.append(b);

	return result;
}

template<typename Traits>
typename Traits::const_pointer
GetBasePathImpl(typename Traits::const_pointer p) noexcept
{
#if !CLANG_CHECK_VERSION(3,6)
	/* disabled on clang due to -Wtautological-pointer-compare */
	assert(p != nullptr);
#endif

	auto sep = Traits::FindLastSeparator(p);
	return sep != nullptr
		? sep + 1
		: p;
}

template<typename Traits>
typename Traits::string_view
GetParentPathImpl(typename Traits::const_pointer p) noexcept
{
#if !CLANG_CHECK_VERSION(3,6)
	/* disabled on clang due to -Wtautological-pointer-compare */
	assert(p != nullptr);
#endif

	auto sep = Traits::FindLastSeparator(p);
	if (sep == nullptr)
		return Traits::CURRENT_DIRECTORY;
	if (sep == p)
		return {p, 1u};
#ifdef _WIN32
	if (Traits::IsDrive(p) && sep == p + 2)
		return {p, 3u};
#endif
	return {p, size_t(sep - p)};
}

template<typename Traits>
typename Traits::string_view
GetParentPathImpl(typename Traits::string_view p) noexcept
{
	auto sep = Traits::FindLastSeparator(p);
	if (sep == nullptr)
		return Traits::CURRENT_DIRECTORY;
	if (sep == p.data())
		return p.substr(0, 1);
#ifdef _WIN32
	if (Traits::IsDrive(p) && sep == p.data() + 2)
		return p.substr(0, 3);
#endif
	return p.substr(0, sep - p.data());
}

template<typename Traits>
typename Traits::const_pointer
RelativePathImpl(typename Traits::string_view base,
		 typename Traits::const_pointer other) noexcept
{
	assert(other != nullptr);

	other = StringAfterPrefix(other, base);
	if (other == nullptr)
		/* mismatch */
		return nullptr;

	if (*other != 0) {
		if (!Traits::IsSeparator(*other)) {
			if (!base.empty() && Traits::IsSeparator(other[-1]))
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

template<typename Traits>
typename Traits::string_view
RelativePathImpl(typename Traits::string_view base,
		 typename Traits::string_view _other) noexcept
{
	BasicStringView<typename Traits::value_type> other(_other);

	if (!other.SkipPrefix(base))
		/* mismatch */
		return {};

	if (!other.empty()) {
		if (!Traits::IsSeparator(other.front())) {
			if (!base.empty() && Traits::IsSeparator(other.data[-1]))
				/* "other" has no more slash, but the
				   matching base ended with a slash:
				   enough to detect a match */
				return other;

			/* mismatch */
			return {};
		}

		/* skip remaining path separators */
		while (!other.empty() && Traits::IsSeparator(other.front()))
			other.pop_front();
	}

	return other;
}

PathTraitsFS::string
PathTraitsFS::Build(string_view a, string_view b) noexcept
{
	return BuildPathImpl<PathTraitsFS>(a, b);
}

PathTraitsFS::const_pointer
PathTraitsFS::GetBase(PathTraitsFS::const_pointer p) noexcept
{
	return GetBasePathImpl<PathTraitsFS>(p);
}

PathTraitsFS::string_view
PathTraitsFS::GetParent(PathTraitsFS::const_pointer p) noexcept
{
	return GetParentPathImpl<PathTraitsFS>(p);
}

PathTraitsFS::string_view
PathTraitsFS::GetParent(string_view p) noexcept
{
	return GetParentPathImpl<PathTraitsFS>(p);
}

PathTraitsFS::const_pointer
PathTraitsFS::Relative(string_view base, const_pointer other) noexcept
{
	return RelativePathImpl<PathTraitsFS>(base, other);
}

PathTraitsFS::string_view
PathTraitsFS::Relative(string_view base, string_view other) noexcept
{
	return RelativePathImpl<PathTraitsFS>(base, other);
}

PathTraitsFS::string
PathTraitsFS::Apply(const_pointer base, const_pointer path) noexcept
{
	// TODO: support the Windows syntax (absolute path with or without drive, drive with relative path)

	if (base == nullptr)
		return path;

	if (IsAbsolute(path))
		return path;

	return Build(base, path);
}

PathTraitsUTF8::string
PathTraitsUTF8::Build(string_view a, string_view b) noexcept
{
	return BuildPathImpl<PathTraitsUTF8>(a, b);
}

bool
PathTraitsUTF8::IsAbsoluteOrHasScheme(const_pointer p) noexcept
{
	return IsAbsolute(p) || uri_has_scheme(p);
}

PathTraitsUTF8::const_pointer
PathTraitsUTF8::GetBase(const_pointer p) noexcept
{
	return GetBasePathImpl<PathTraitsUTF8>(p);
}

PathTraitsUTF8::string_view
PathTraitsUTF8::GetParent(const_pointer p) noexcept
{
	return GetParentPathImpl<PathTraitsUTF8>(p);
}

PathTraitsUTF8::string_view
PathTraitsUTF8::GetParent(string_view p) noexcept
{
	return GetParentPathImpl<PathTraitsUTF8>(p);
}

PathTraitsUTF8::const_pointer
PathTraitsUTF8::Relative(string_view base, const_pointer other) noexcept
{
	return RelativePathImpl<PathTraitsUTF8>(base, other);
}

PathTraitsUTF8::string_view
PathTraitsUTF8::Relative(string_view base, string_view other) noexcept
{
	return RelativePathImpl<PathTraitsUTF8>(base, other);
}
