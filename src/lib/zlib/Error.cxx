// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "Error.hxx"

#include <zlib.h>

const char *
ZlibError::what() const noexcept
{
	return zError(code);
}
