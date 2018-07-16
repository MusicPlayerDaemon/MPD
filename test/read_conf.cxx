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

#include "config.h"
#include "config/Global.hxx"
#include "fs/Path.hxx"
#include "Log.hxx"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
try {
	if (argc != 3) {
		fprintf(stderr, "Usage: read_conf FILE SETTING\n");
		return EXIT_FAILURE;
	}

	const Path config_path = Path::FromFS(argv[1]);
	const char *name = argv[2];

	config_global_init();

	ReadConfigFile(config_path);

	ConfigOption option = ParseConfigOptionName(name);
	const char *value = option != ConfigOption::MAX
		? config_get_string(option)
		: nullptr;
	int ret;
	if (value != NULL) {
		printf("%s\n", value);
		ret = EXIT_SUCCESS;
	} else {
		fprintf(stderr, "No such setting: %s\n", name);
		ret = EXIT_FAILURE;
	}

	config_global_finish();
	return ret;
 } catch (const std::exception &e) {
	LogError(e);
	return EXIT_FAILURE;
 }
