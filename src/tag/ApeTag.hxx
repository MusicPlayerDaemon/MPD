// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_APE_TAG_HXX
#define MPD_APE_TAG_HXX

class InputStream;
class TagHandler;

/**
 * Scan the APE tags of a stream.
 *
 * Throws on I/O error.
 *
 * @param path_fs the path of the file in filesystem encoding
 */
bool
tag_ape_scan2(InputStream &is, TagHandler &handler);

#endif
