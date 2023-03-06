// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_TAG_ANY_HXX
#define MPD_TAG_ANY_HXX

class Client;
class TagHandler;

/**
 * Scan tags in the song file specified by the given URI.  The URI may
 * be relative to the music directory (the "client" parameter will be
 * used to obtain a handle to the #Storage) or absolute.
 *
 * Throws on error.
 */
void
TagScanAny(Client &client, const char *uri, TagHandler &handler);

#endif
