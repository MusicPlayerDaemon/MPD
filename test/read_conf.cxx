// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "config/Data.hxx"
#include "config/Param.hxx"
#include "config/File.hxx"
#include "lib/fmt/RuntimeError.hxx"
#include "fs/Path.hxx"
#include "fs/NarrowPath.hxx"
#include "util/PrintException.hxx"

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
		throw FmtRuntimeError("Unknown setting: {}", name);

	ConfigData config;
	ReadConfigFile(config, config_path);

	const auto *param = config.GetParam(option);
	if (param == nullptr)
		throw FmtRuntimeError("No such setting: {}", name);

	printf("%s\n", param->value.c_str());
	return EXIT_SUCCESS;
} catch (...) {
	PrintException(std::current_exception());
	return EXIT_FAILURE;
}
