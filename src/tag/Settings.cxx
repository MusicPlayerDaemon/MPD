// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Settings.hxx"
#include "Type.hxx"

TagMask global_tag_mask = TagMask::All() & ~TagMask(TAG_COMMENT);
