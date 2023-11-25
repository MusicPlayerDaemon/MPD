// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include "LineReader.hxx"
#include "FileReader.hxx"
#include "BufferedReader.hxx"

class FileLineReader final : public LineReader {
	FileReader file_reader;
	BufferedReader buffered_reader;

public:
	explicit FileLineReader(Path path_fs)
		:file_reader(path_fs),
		 buffered_reader(file_reader) {}

	/* virtual methods from class LineReader */
	char *ReadLine() override {
		return buffered_reader.ReadLine();
	}
};
