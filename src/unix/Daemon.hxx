// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_DAEMON_HXX
#define MPD_DAEMON_HXX

class AllocatedPath;

/**
 * Throws on error.
 */
#ifndef _WIN32
void
daemonize_init(const char *user, const char *group, AllocatedPath &&pidfile);
#else
static inline void
daemonize_init(const char *user, const char *group, AllocatedPath &&pidfile)
{ (void)user; (void)group; (void)pidfile; }
#endif

#ifndef _WIN32
void
daemonize_finish() noexcept;
#else
static inline void
daemonize_finish() noexcept
{ /* nop */ }
#endif

/**
 * Kill the MPD which is currently running, pid determined from the
 * pid file.
 *
 * Throws on error.
 */
#ifndef _WIN32
[[noreturn]]
void
daemonize_kill();
#else
#include <stdexcept>
[[noreturn]]
static inline void
daemonize_kill()
{
	throw std::runtime_error("--kill is not available on WIN32");
}
#endif

/**
 * Write the pid file (if one is configured).
 *
 * Throws on error.
 */
#ifndef _WIN32
void
daemonize_commit();
#else
static inline void
daemonize_commit() {}
#endif

#endif
