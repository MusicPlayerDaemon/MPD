/*
 * Copyright (C) 2003-2009 The Music Player Daemon Project
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

#include "config.h"
#include "permission.h"
#include "conf.h"

#include <glib.h>

#include <stdbool.h>
#include <string.h>

#define PERMISSION_PASSWORD_CHAR	'@'
#define PERMISSION_SEPERATOR		","

#define PERMISSION_READ_STRING		"read"
#define PERMISSION_ADD_STRING		"add"
#define PERMISSION_CONTROL_STRING	"control"
#define PERMISSION_ADMIN_STRING		"admin"

static GHashTable *permission_passwords;

static unsigned permission_default;

static unsigned parsePermissions(const char *string)
{
	unsigned permission = 0;
	gchar **tokens;

	if (!string)
		return 0;

	tokens = g_strsplit(string, PERMISSION_SEPERATOR, 0);
	for (unsigned i = 0; tokens[i] != NULL; ++i) {
		char *temp = tokens[i];

		if (strcmp(temp, PERMISSION_READ_STRING) == 0) {
			permission |= PERMISSION_READ;
		} else if (strcmp(temp, PERMISSION_ADD_STRING) == 0) {
			permission |= PERMISSION_ADD;
		} else if (strcmp(temp, PERMISSION_CONTROL_STRING) == 0) {
			permission |= PERMISSION_CONTROL;
		} else if (strcmp(temp, PERMISSION_ADMIN_STRING) == 0) {
			permission |= PERMISSION_ADMIN;
		} else {
			g_error("unknown permission \"%s\"", temp);
		}
	}

	g_strfreev(tokens);

	return permission;
}

void initPermissions(void)
{
	char *password;
	unsigned permission;
	const struct config_param *param;

	permission_passwords = g_hash_table_new_full(g_str_hash, g_str_equal,
						     g_free, NULL);

	permission_default = PERMISSION_READ | PERMISSION_ADD |
	    PERMISSION_CONTROL | PERMISSION_ADMIN;

	param = config_get_next_param(CONF_PASSWORD, NULL);

	if (param) {
		permission_default = 0;

		do {
			const char *separator =
				strchr(param->value, PERMISSION_PASSWORD_CHAR);

			if (separator == NULL)
				g_error("\"%c\" not found in password string "
					"\"%s\", line %i",
					PERMISSION_PASSWORD_CHAR,
					param->value, param->line);

			password = g_strndup(param->value,
					     separator - param->value);

			permission = parsePermissions(separator + 1);

			g_hash_table_replace(permission_passwords,
					     password,
					     GINT_TO_POINTER(permission));
		} while ((param = config_get_next_param(CONF_PASSWORD, param)));
	}

	param = config_get_param(CONF_DEFAULT_PERMS);

	if (param)
		permission_default = parsePermissions(param->value);
}

int getPermissionFromPassword(char const* password, unsigned* permission)
{
	bool found;
	gpointer key, value;

	found = g_hash_table_lookup_extended(permission_passwords,
					     password, &key, &value);
	if (!found)
		return -1;

	*permission = GPOINTER_TO_INT(value);
	return 0;
}

void finishPermissions(void)
{
	g_hash_table_destroy(permission_passwords);
}

unsigned getDefaultPermissions(void)
{
	return permission_default;
}
