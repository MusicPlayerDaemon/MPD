// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_LOG_INIT_HXX
#define MPD_LOG_INIT_HXX

struct ConfigData;

/**
 * Configure a logging destination for daemon startup, before the
 * configuration file is read.  This allows the daemon to use the
 * logging library (and the command line verbose level) before it's
 * daemonized.
 *
 * @param verbose true when the program is started with --verbose
 */
void
log_early_init(bool verbose) noexcept;

/**
 * Throws #std::runtime_error on error.
 */
void
log_init(const ConfigData &config, bool verbose, bool use_stdout);

void
log_deinit() noexcept;

void
setup_log_output();

int
cycle_log_files() noexcept;

#endif /* LOG_H */
