// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Path.hxx"
#include "AllocatedPath.hxx"

AllocatedPath
Path::GetDirectoryName() const noexcept
{
	return AllocatedPath::FromFS(PathTraitsFS::GetParent(c_str()));
}

AllocatedPath
Path::WithSuffix(const_pointer new_suffix) const noexcept
{
	AllocatedPath result{*this};
	result.SetSuffix(new_suffix);
	return result;
}

AllocatedPath
operator+(Path a, PathTraitsFS::string_view b) noexcept
{
	return AllocatedPath::Concat(a.c_str(), b);
}

AllocatedPath
operator/(Path a, Path b) noexcept
{
	return AllocatedPath::Build(a, b);
}
