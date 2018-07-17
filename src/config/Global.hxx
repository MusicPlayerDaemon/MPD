/*
 * Copyright 2003-2017 The Music Player Daemon Project
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

#ifndef MPD_CONFIG_GLOBAL_HXX
#define MPD_CONFIG_GLOBAL_HXX

#include "Option.hxx"
#include "Compiler.h"

class Path;
struct ConfigData;
struct ConfigParam;
struct ConfigBlock;

void
config_global_init();

void
config_global_finish();

gcc_const
const ConfigData &
GetGlobalConfig() noexcept;

/**
 * Call this function after all configuration has been evaluated.  It
 * checks for unused parameters, and logs warnings.
 */
void
config_global_check();

void
ReadConfigFile(Path path);

gcc_pure
const ConfigParam *
config_get_param(enum ConfigOption option) noexcept;

const char *
config_get_string(enum ConfigOption option,
		  const char *default_value=nullptr) noexcept;

unsigned
config_get_positive(enum ConfigOption option, unsigned default_value);

bool config_get_bool(enum ConfigOption option, bool default_value);

#endif
