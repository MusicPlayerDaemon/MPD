// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "TruncateString.hxx"
#include "Compiler.h"

#include <algorithm>

#include <string.h>

char *
CopyTruncateString(char *gcc_restrict dest, const char *gcc_restrict src,
		   size_t size) noexcept
{
	size_t length = strlen(src);
	if (length >= size)
		length = size - 1;

	char *p = std::copy_n(src, length, dest);
	*p = '\0';
	return p;
}
