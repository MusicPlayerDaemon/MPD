/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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

class Error;
class Path;
class AllocatedPath;
struct config_param;

void config_global_init(void);
void config_global_finish(void);

/**
 * Call this function after all configuration has been evaluated.  It
 * checks for unused parameters, and logs warnings.
 */
void config_global_check(void);

bool
ReadConfigFile(Path path, Error &error);

gcc_pure
const config_param *
config_get_param(enum ConfigOption option);

/**
 * Find a block with a matching attribute.
 *
 * @param option the blocks to search
 * @param key the attribute name
 * @param value the expected attribute value
 */
gcc_pure
const config_param *
config_find_block(ConfigOption option, const char *key, const char *value);

/* Note on gcc_pure: Some of the functions declared pure are not
   really pure in strict sense.  They have side effect such that they
   validate parameter's value and signal an error if it's invalid.
   However, if the argument was already validated or we don't care
   about the argument at all, this may be ignored so in the end, we
   should be fine with calling those functions pure.  */

gcc_pure
const char *
config_get_string(enum ConfigOption option, const char *default_value);

/**
 * Returns an optional configuration variable which contains an
 * absolute path.  If there is a tilde prefix, it is expanded.
 * Returns AllocatedPath::Null() if the value is not present.  If the path
 * could not be parsed, returns AllocatedPath::Null() and sets the error.
 */
AllocatedPath
config_get_path(enum ConfigOption option, Error &error);

/**
 * Parse a configuration parameter as a path.
 * If there is a tilde prefix, it is expanded. If the path could
 * not be parsed, returns AllocatedPath::Null() and sets the error.
 */
AllocatedPath
config_parse_path(const struct config_param *param, Error & error_r);

gcc_pure
unsigned
config_get_unsigned(enum ConfigOption option, unsigned default_value);

gcc_pure
unsigned
config_get_positive(enum ConfigOption option, unsigned default_value);

gcc_pure
bool config_get_bool(enum ConfigOption option, bool default_value);

#endif
