/*
 * Copyright 2003-2022 The Music Player Daemon Project
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

#pragma once

#include "config.h"

#ifdef HAVE_ICU
#define HAVE_ICU_CANONICALIZE

#include <string_view>

class AllocatedString;

/**
 * Throws on error.
 */
void
IcuCanonicalizeInit();

void
IcuCanonicalizeFinish() noexcept;

/**
 * Transform the given string to "canonical" form to allow fuzzy
 * string comparisons.  The full set of features (if ICU is being
 * used):
 *
 * - case folding (optional)
 */
AllocatedString
IcuCanonicalize(std::string_view src, bool fold_case) noexcept;

#endif
