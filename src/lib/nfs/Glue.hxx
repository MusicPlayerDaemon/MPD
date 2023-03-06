// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_NFS_GLUE_HXX
#define MPD_NFS_GLUE_HXX

class EventLoop;
class NfsConnection;

void
nfs_init(EventLoop &event_loop);

void
nfs_finish() noexcept;

/**
 * Return the EventLoop that was passed to nfs_init().
 */
[[gnu::const]]
EventLoop &
nfs_get_event_loop() noexcept;

[[gnu::pure]]
NfsConnection &
nfs_get_connection(const char *server, const char *export_name) noexcept;

#endif
