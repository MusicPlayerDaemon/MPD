// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "AllocatedPath.hxx"
#include "Charset.hxx"
#include "Features.hxx"

/* no inlining, please */
AllocatedPath::~AllocatedPath() noexcept = default;

AllocatedPath
AllocatedPath::FromUTF8(std::string_view path_utf8) noexcept
{
#ifdef FS_CHARSET_ALWAYS_UTF8
	return FromFS(path_utf8);
#else
	try {
		return {::PathFromUTF8(path_utf8)};
	} catch (...) {
		return nullptr;
	}
#endif
}

AllocatedPath
AllocatedPath::FromUTF8Throw(std::string_view path_utf8)
{
#ifdef FS_CHARSET_ALWAYS_UTF8
	return FromFS(path_utf8);
#else
	return {::PathFromUTF8(path_utf8)};
#endif
}

void
AllocatedPath::SetSuffix(const_pointer new_suffix) noexcept
{
	assert(new_suffix != nullptr);
	assert(*new_suffix == '.');

	const auto end = value.end();
	auto begin = end;

	if (auto old = GetSuffix())
		begin = std::next(value.begin(), old - value.data());

	value.replace(begin, end, new_suffix);
}

void
AllocatedPath::ChopSeparators() noexcept
{
	size_t l = length();
	const auto *p = data();

	while (l >= 2 && PathTraitsFS::IsSeparator(p[l - 1])) {
		--l;

		value.pop_back();
	}
}
