// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include <string_view>

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

/**
 * Throws on error.
 */
[[nodiscard]]
NfsConnection &
nfs_make_connection(const char *url);

/**
 * Throws on error.
 */
[[nodiscard]]
NfsConnection &
nfs_get_connection(std::string_view server,
		   std::string_view export_name);
