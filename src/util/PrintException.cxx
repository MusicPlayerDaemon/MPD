// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "PrintException.hxx"

#include <stdio.h>

void
PrintException(const std::exception &e) noexcept
{
	fprintf(stderr, "%s\n", e.what());
	try {
		std::rethrow_if_nested(e);
	} catch (const std::exception &nested) {
		PrintException(nested);
	} catch (const char *s) {
		fprintf(stderr, "%s\n", s);
	} catch (...) {
		fprintf(stderr, "Unrecognized nested exception\n");
	}
}

void
PrintException(const std::exception_ptr &ep) noexcept
{
	try {
		std::rethrow_exception(ep);
	} catch (const std::exception &e) {
		PrintException(e);
	} catch (const char *s) {
		fprintf(stderr, "%s\n", s);
	} catch (...) {
		fprintf(stderr, "Unrecognized exception\n");
	}
}
