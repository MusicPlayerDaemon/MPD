// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

class LineReader
{
public:
	/**
	 * Reads a line from the input file, and strips trailing
	 * space.  There is a reasonable maximum line length, only to
	 * prevent denial of service.
	 *
	 * @return a pointer to the line, or nullptr on end-of-file
	 */
	virtual char *ReadLine() = 0;
};
