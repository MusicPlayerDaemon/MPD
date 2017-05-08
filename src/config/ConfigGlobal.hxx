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

#include "ConfigOption.hxx"
#include "Compiler.h"

#include <chrono>

class Path;
class AllocatedPath;
struct ConfigParam;
struct ConfigBlock;

void
config_global_init();

void
config_global_finish();

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

gcc_pure
const ConfigBlock *
config_get_block(enum ConfigBlockOption option) noexcept;

/**
 * Find a block with a matching attribute.
 *
 * @param option the blocks to search
 * @param key the attribute name
 * @param value the expected attribute value
 */
gcc_pure
const ConfigBlock *
config_find_block(ConfigBlockOption option, const char *key, const char *value);

/* Note on gcc_pure: Some of the functions declared pure are not
   really pure in strict sense.  They have side effect such that they
   validate parameter's value and signal an error if it's invalid.
   However, if the argument was already validated or we don't care
   about the argument at all, this may be ignored so in the end, we
   should be fine with calling those functions pure.  */

gcc_pure
const char *
config_get_string(enum ConfigOption option,
		  const char *default_value=nullptr) noexcept;

/**
 * Returns an optional configuration variable which contains an
 * absolute path.  If there is a tilde prefix, it is expanded.
 * Returns AllocatedPath::Null() if the value is not present.
 *
 * Throws #std::runtime_error on error.
 */
AllocatedPath
config_get_path(enum ConfigOption option);

gcc_pure
unsigned
config_get_unsigned(enum ConfigOption option, unsigned default_value);

gcc_pure
static inline std::chrono::steady_clock::duration
config_get_unsigned(ConfigOption option,
		    std::chrono::steady_clock::duration default_value)
{
	// TODO: allow unit suffixes
	auto u = config_get_unsigned(option, default_value.count());
	return std::chrono::steady_clock::duration(u);
}

gcc_pure
unsigned
config_get_positive(enum ConfigOption option, unsigned default_value);

gcc_pure
static inline std::chrono::steady_clock::duration
config_get_positive(ConfigOption option,
		    std::chrono::steady_clock::duration default_value)
{
	// TODO: allow unit suffixes
	auto u = config_get_positive(option, default_value.count());
	return std::chrono::steady_clock::duration(u);
}

gcc_pure
bool config_get_bool(enum ConfigOption option, bool default_value);

#endif
