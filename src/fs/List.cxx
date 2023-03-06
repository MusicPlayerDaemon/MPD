// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

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
