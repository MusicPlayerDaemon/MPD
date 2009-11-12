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
#include "conf.h"

#include <glib.h>

#include <assert.h>

static void
my_log_func(G_GNUC_UNUSED const gchar *log_domain,
	    GLogLevelFlags log_level,
	    const gchar *message, G_GNUC_UNUSED gpointer user_data)
{
	if (log_level > G_LOG_LEVEL_WARNING)
		return;

	g_printerr("%s\n", message);
}

int main(int argc, char **argv)
{
	const char *path, *name, *value;
	GError *error = NULL;
	bool success;
	int ret;

	if (argc != 3) {
		g_printerr("Usage: read_conf FILE SETTING\n");
		return 1;
	}

	path = argv[1];
	name = argv[2];

	g_log_set_default_handler(my_log_func, NULL);

	config_global_init();
	success = config_read_file(path, &error);
	if (!success) {
		g_printerr("%s:", error->message);
		g_error_free(error);
		return 1;
	}

	value = config_get_string(name, NULL);
	if (value != NULL) {
		g_print("%s\n", value);
		ret = 0;
	} else {
		g_printerr("No such setting: %s\n", name);
		ret = 2;
	}

	config_global_finish();
	return 0;
}
