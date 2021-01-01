/*
 * Copyright 2003-2021 The Music Player Daemon Project
 * http://www.musicpd.org
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

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
