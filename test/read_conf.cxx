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

#include "config/Data.hxx"
#include "config/Param.hxx"
#include "config/File.hxx"
#include "fs/Path.hxx"
#include "fs/NarrowPath.hxx"
#include "util/PrintException.hxx"
#include "util/RuntimeError.hxx"

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
try {
	if (argc != 3) {
		fprintf(stderr, "Usage: read_conf FILE SETTING\n");
		return EXIT_FAILURE;
	}

	const FromNarrowPath config_path = argv[1];
	const char *name = argv[2];

	const auto option = ParseConfigOptionName(name);
	if (option == ConfigOption::MAX)
		throw FormatRuntimeError("Unknown setting: %s", name);

	ConfigData config;
	ReadConfigFile(config, config_path);

	const auto *param = config.GetParam(option);
	if (param == nullptr)
		throw FormatRuntimeError("No such setting: %s", name);

	printf("%s\n", param->value.c_str());
	return EXIT_SUCCESS;
} catch (...) {
	PrintException(std::current_exception());
	return EXIT_FAILURE;
}
