/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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

#ifndef MPD_STDIO_OUTPUT_STREAM_HXX
#define MPD_STDIO_OUTPUT_STREAM_HXX

#include "check.h"
#include "OutputStream.hxx"
#include "fs/AllocatedPath.hxx"
#include "Compiler.h"

#include <stdio.h>

class StdioOutputStream final : public OutputStream {
	FILE *const file;

public:
	StdioOutputStream(FILE *_file):file(_file) {}

	/* virtual methods from class OutputStream */
	bool Write(const void *data, size_t size,
		   gcc_unused Error &error) override {
		fwrite(data, 1, size, file);

		/* this class is debug-only and ignores errors */
		return true;
	}
};

#endif
