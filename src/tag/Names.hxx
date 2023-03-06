// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include "Type.hxx"

#include <array>

/**
 * An array of strings, which map the #TagType to its machine
 * readable name (specific to the MPD protocol).
 */
extern const std::array<const char *, TAG_NUM_OF_ITEM_TYPES> tag_item_names;
