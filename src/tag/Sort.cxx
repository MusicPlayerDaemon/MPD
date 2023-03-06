// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Sort.hxx"
#include "Tag.hxx"

#include <algorithm>

#include <string.h>
#include <stdlib.h>

[[gnu::pure]]
static bool
CompareNumeric(const char *a, const char *b) noexcept
{
	long a_value = strtol(a, nullptr, 10);
	long b_value = strtol(b, nullptr, 10);

	return a_value < b_value;
}

bool
CompareTags(TagType type, bool descending, const Tag &a, const Tag &b) noexcept
{
	const char *a_value = a.GetSortValue(type);
	const char *b_value = b.GetSortValue(type);

	if (descending) {
		using std::swap;
		swap(a_value, b_value);
	}

	switch (type) {
	case TAG_DISC:
	case TAG_TRACK:
		return CompareNumeric(a_value, b_value);

	default:
		return strcmp(a_value, b_value) < 0;
	}
}
