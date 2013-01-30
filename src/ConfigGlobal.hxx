/*
 * Copyright (C) 2003-2013 The Music Player Daemon Project
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
#include "gerror.h"
#include "gcc.h"

#include <stdbool.h>
#include <stddef.h>

#define DEFAULT_PLAYLIST_MAX_LENGTH (1024*16)
#define DEFAULT_PLAYLIST_SAVE_ABSOLUTE_PATHS false

#ifdef __cplusplus
class Path;
#endif

void config_global_init(void);
void config_global_finish(void);

/**
 * Call this function after all configuration has been evaluated.  It
 * checks for unused parameters, and logs warnings.
 */
void config_global_check(void);

#ifdef __cplusplus

bool
ReadConfigFile(const Path &path, GError **error_r);

#endif

#ifdef __cplusplus
extern "C" {
#endif

/* don't free the returned value
   set _last_ to NULL to get first entry */
gcc_pure
const struct config_param *
config_get_next_param(enum ConfigOption option,
		      const struct config_param *last);

gcc_pure
static inline const struct config_param *
config_get_param(enum ConfigOption option)
{
	return config_get_next_param(option, NULL);
}

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
 * Returns NULL if the value is not present.  If the path could not be
 * parsed, returns NULL and sets the error.
 *
 * The return value must be freed with g_free().
 */
gcc_malloc
char *
config_dup_path(enum ConfigOption option, GError **error_r);

gcc_pure
unsigned
config_get_unsigned(enum ConfigOption option, unsigned default_value);

gcc_pure
unsigned
config_get_positive(enum ConfigOption option, unsigned default_value);

gcc_pure
bool config_get_bool(enum ConfigOption option, bool default_value);

#ifdef __cplusplus
}
#endif

#endif
