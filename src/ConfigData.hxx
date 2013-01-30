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

#ifndef MPD_CONFIG_DATA_HXX
#define MPD_CONFIG_DATA_HXX

#include "ConfigOption.hxx"
#include "gerror.h"
#include "gcc.h"

#ifdef __cplusplus
#include <glib.h>
#include <string>
#include <array>
#include <vector>
#endif

#include <stdbool.h>

#ifdef __cplusplus

struct block_param {
	std::string name;
	std::string value;
	int line;

	/**
	 * This flag is false when nobody has queried the value of
	 * this option yet.
	 */
	mutable bool used;

	gcc_nonnull_all
	block_param(const char *_name, const char *_value, int _line=-1)
		:name(_name), value(_value), line(_line), used(false) {}
};

#endif

struct config_param {
	char *value;
	unsigned int line;

#ifdef __cplusplus
	std::vector<block_param> block_params;

	/**
	 * This flag is false when nobody has queried the value of
	 * this option yet.
	 */
	bool used;

	config_param(int _line=-1)
		:value(nullptr), line(_line), used(false) {}
	config_param(const char *_value, int _line=-1);
	~config_param();
#endif
};

#ifdef __cplusplus

struct ConfigData {
	std::array<GSList *, std::size_t(CONF_MAX)> params;
};

#endif

#ifdef __cplusplus
extern "C" {
#endif

void
config_add_block_param(struct config_param * param, const char *name,
		       const char *value, int line);

gcc_pure
const struct block_param *
config_get_block_param(const struct config_param *param, const char *name);

gcc_pure
const char *
config_get_block_string(const struct config_param *param, const char *name,
			const char *default_value);

gcc_malloc
char *
config_dup_block_string(const struct config_param *param, const char *name,
			const char *default_value);

/**
 * Same as config_dup_path(), but looks up the setting in the
 * specified block.
 */
gcc_malloc
char *
config_dup_block_path(const struct config_param *param, const char *name,
		      GError **error_r);

gcc_pure
unsigned
config_get_block_unsigned(const struct config_param *param, const char *name,
			  unsigned default_value);

gcc_pure
bool
config_get_block_bool(const struct config_param *param, const char *name,
		      bool default_value);

#ifdef __cplusplus
}
#endif

#endif
