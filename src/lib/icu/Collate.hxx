// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_ICU_COLLATE_HXX
#define MPD_ICU_COLLATE_HXX

#include <string_view>

/**
 * Throws #std::runtime_error on error.
 */
void
IcuCollateInit();

void
IcuCollateFinish() noexcept;

[[gnu::pure]]
int
IcuCollate(std::string_view a, std::string_view b) noexcept;

#endif
