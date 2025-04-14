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
main(int argc, [[maybe_unused]] char **argv)
{
	if (argc != 1)
		return EXIT_FAILURE;

	std::map<std::string_view, TagType> names;
	for (unsigned i = 0; i < unsigned(TAG_NUM_OF_ITEM_TYPES); ++i)
		names[tag_item_names[i]] = TagType(i);

	printf("#include \"ParseName.hxx\"\n"
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
	       "  switch (*name++) {\n");

	char first = 0;

	for (const auto &[name, tag] : names) {
		if (name.front() != first) {
			if (first != 0)
				printf("    break;\n\n");
			first = name.front();
			printf("  case '%c':\n", first);
		}

		printf("    if (strcmp(name, ((const char *)\"%.*s\") + 1) == 0) return TagType(%u);\n",
		       int(name.size()), name.data(), unsigned(tag));
	}

	printf("    break;\n\n");

	printf("  }\n"
	       "\n"
	       "  return TAG_NUM_OF_ITEM_TYPES;\n"
	       "}\n");

	return EXIT_SUCCESS;
}
