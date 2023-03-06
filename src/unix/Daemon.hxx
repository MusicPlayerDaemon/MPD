// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_DAEMON_HXX
#define MPD_DAEMON_HXX

class AllocatedPath;

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
daemonize_finish();
#else
static inline void
daemonize_finish()
{ /* nop */ }
#endif

/**
 * Kill the MPD which is currently running, pid determined from the
 * pid file.
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
 * Close stdin (fd 0) and re-open it as /dev/null.
 */
#ifndef _WIN32
void
daemonize_close_stdin();
#else
static inline void
daemonize_close_stdin() {}
#endif

/**
 * Change to the configured Unix user.
 */
#ifndef _WIN32
void
daemonize_set_user();
#else
static inline void
daemonize_set_user()
{ /* nop */ }
#endif

#ifndef _WIN32
void
daemonize_begin(bool detach);
#else
static inline void
daemonize_begin(bool detach)
{ (void)detach; }
#endif

#ifndef _WIN32
void
daemonize_commit();
#else
static inline void
daemonize_commit() {}
#endif

#endif
