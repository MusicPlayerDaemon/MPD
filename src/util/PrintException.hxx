// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#ifndef PRINT_EXCEPTION_HXX
#define PRINT_EXCEPTION_HXX

#include <exception>

/**
 * Print this exception (and its nested exceptions, if any) to stderr.
 */
void
PrintException(const std::exception &e) noexcept;

void
PrintException(const std::exception_ptr &ep) noexcept;

#endif
