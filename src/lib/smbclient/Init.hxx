// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_SMBCLIENT_INIT_HXX
#define MPD_SMBCLIENT_INIT_HXX

/**
 * Initialize libsmbclient.
 *
 * Throws std::runtime_error on error.
 */
void
SmbclientInit();

#endif
