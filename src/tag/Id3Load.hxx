// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_TAG_ID3_LOAD_HXX
#define MPD_TAG_ID3_LOAD_HXX

#include "Id3Unique.hxx"

class InputStream;

/**
 * Loads the ID3 tags from the #InputStream into a libid3tag object.
 *
 * @return nullptr on error or if no ID3 tag was found in the file
 */
UniqueId3Tag
tag_id3_load(InputStream &is);

#endif
