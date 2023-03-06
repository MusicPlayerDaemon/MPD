// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_TAG_SAVE_HXX
#define MPD_TAG_SAVE_HXX

struct Tag;
class BufferedOutputStream;

void
tag_save(BufferedOutputStream &os, const Tag &tag);

#endif
