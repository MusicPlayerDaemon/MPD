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

#include "List.hxx"
#include "AllocatedPath.hxx"
#include "Glob.hxx"

#ifdef HAVE_CLASS_GLOB
#include "DirectoryReader.hxx"

#include <algorithm>
#include <vector>
#endif

std::forward_list<AllocatedPath>
ListWildcard(Path pattern)
{
#ifdef HAVE_CLASS_GLOB
	const auto base = pattern.GetBase();
	if (StringFindAny(base.c_str(), PATH_LITERAL("*?")) != nullptr) {
		const Glob glob(base.ToUTF8Throw().c_str());

		std::vector<AllocatedPath> v;
		v.reserve(256);

		const auto directory_path = pattern.GetDirectoryName();
		DirectoryReader reader(directory_path);
		while (reader.ReadEntry()) {
			const Path name_fs = reader.GetEntry();

			try {
				if (glob.Check(name_fs.ToUTF8Throw().c_str()))
					v.emplace_back(directory_path / name_fs);
			} catch (...) {
			}
		}

		// TODO: proper Unicode collation?
		std::sort(v.begin(), v.end(),
			  [](const Path a, const Path b){
				  return StringCompare(a.c_str(), b.c_str()) < 0;
			  });

		return {v.begin(), v.end()};
	}
#endif

	return {AllocatedPath(pattern)};
}
