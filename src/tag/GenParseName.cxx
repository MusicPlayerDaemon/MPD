// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Names.hxx"

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
		"#include \"Type.hxx\"\n"
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
