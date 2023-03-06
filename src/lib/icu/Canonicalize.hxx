// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

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
