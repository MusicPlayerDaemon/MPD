// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

/**
 * Parse a DSCP (Differentiated Services Code Point) class name.
 * This can either be a name (CS*, LE, AF*, EF) or numeric (decimal or
 * hexadecimal).
 *
 * @return the DSCP or -1 on error
 */
[[gnu::pure]]
int
ParseDscpClass(const char *s) noexcept;
