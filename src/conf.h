/*
 * Copyright (C) 2003-2011 The Music Player Daemon Project
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

#ifndef MPD_CONF_H
#define MPD_CONF_H

#include "ConfigOption.hxx"
#include "gcc.h"

#include <stdbool.h>
#include <glib.h>

#define DEFAULT_PLAYLIST_MAX_LENGTH (1024*16)
#define DEFAULT_PLAYLIST_SAVE_ABSOLUTE_PATHS false

#define MAX_FILTER_CHAIN_LENGTH 255

#ifdef __cplusplus
class Path;
#endif

struct block_param {
	char *name;
	char *value;
	int line;

	/**
	 * This flag is false when nobody has queried the value of
	 * this option yet.
	 */
	bool used;
};

struct config_param {
	char *value;
	unsigned int line;

	struct block_param *block_params;
	unsigned num_block_params;

	/**
	 * This flag is false when nobody has queried the value of
	 * this option yet.
	 */
	bool used;
};

/**
 * A GQuark for GError instances, resulting from malformed
 * configuration.
 */
G_GNUC_CONST
static inline GQuark
config_quark(void)
{
	return g_quark_from_static_string("config");
}

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

G_BEGIN_DECLS

/* don't free the returned value
   set _last_ to NULL to get first entry */
G_GNUC_PURE
const struct config_param *
config_get_next_param(enum ConfigOption option,
		      const struct config_param *last);

G_GNUC_PURE
static inline const struct config_param *
config_get_param(enum ConfigOption option)
{
	return config_get_next_param(option, NULL);
}

/* Note on G_GNUC_PURE: Some of the functions declared pure are not
   really pure in strict sense.  They have side effect such that they
   validate parameter's value and signal an error if it's invalid.
   However, if the argument was already validated or we don't care
   about the argument at all, this may be ignored so in the end, we
   should be fine with calling those functions pure.  */

G_GNUC_PURE
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
G_GNUC_MALLOC
char *
config_dup_path(enum ConfigOption option, GError **error_r);

G_GNUC_PURE
unsigned
config_get_unsigned(enum ConfigOption option, unsigned default_value);

G_GNUC_PURE
unsigned
config_get_positive(enum ConfigOption option, unsigned default_value);

G_GNUC_PURE
const struct block_param *
config_get_block_param(const struct config_param *param, const char *name);

G_GNUC_PURE
bool config_get_bool(enum ConfigOption option, bool default_value);

G_GNUC_PURE
const char *
config_get_block_string(const struct config_param *param, const char *name,
			const char *default_value);

G_GNUC_MALLOC
static inline char *
config_dup_block_string(const struct config_param *param, const char *name,
			const char *default_value)
{
	return g_strdup(config_get_block_string(param, name, default_value));
}

/**
 * Same as config_dup_path(), but looks up the setting in the
 * specified block.
 */
G_GNUC_MALLOC
char *
config_dup_block_path(const struct config_param *param, const char *name,
		      GError **error_r);

G_GNUC_PURE
unsigned
config_get_block_unsigned(const struct config_param *param, const char *name,
			  unsigned default_value);

G_GNUC_PURE
bool
config_get_block_bool(const struct config_param *param, const char *name,
		      bool default_value);

G_GNUC_MALLOC
struct config_param *
config_new_param(const char *value, int line);

void
config_param_free(struct config_param *param);

void
config_add_block_param(struct config_param * param, const char *name,
		       const char *value, int line);

G_END_DECLS

#endif
