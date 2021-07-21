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

#include "Type.h"

#include <cstdlib>
#include <map>
#include <string_view>

#include <stdio.h>

/*

  This program generates an optimized parser for tag names, by doign
  switch() on the first character.  This reduces the number of
  strcmp() calls.

 */

int
main(int argc, char **argv)
{
	if (argc != 2)
		return EXIT_FAILURE;

	FILE *out = fopen(argv[1], "w");

	std::map<std::string_view, TagType> names;
	for (unsigned i = 0; i < unsigned(TAG_NUM_OF_ITEM_TYPES); ++i)
		names[tag_item_names[i]] = TagType(i);

	fprintf(out,
		"#include \"ParseName.hxx\"\n"
		"\n"
		"#include <assert.h>\n"
		"#include <string.h>\n"
		"\n"
		"TagType\n"
		"tag_name_parse(const char *name) noexcept\n"
		"{\n"
		"  assert(name != nullptr);\n"
		"\n"
		"  switch (*name) {\n");

	char first = 0;

	for (const auto &[name, tag] : names) {
		if (name.front() != first) {
			if (first != 0)
				fprintf(out, "    break;\n\n");
			first = name.front();
			fprintf(out, "  case '%c':\n", first);
		}

		fprintf(out,
			"    if (strcmp(name + 1, \"%.*s\") == 0) return TagType(%u);\n",
			int(name.size() - 1), name.data() + 1, unsigned(tag));
	}

	fprintf(out, "    break;\n\n");

	fprintf(out,
		"  }\n"
		"\n"
		"  return TAG_NUM_OF_ITEM_TYPES;\n"
		"}\n");

	return EXIT_SUCCESS;
}
