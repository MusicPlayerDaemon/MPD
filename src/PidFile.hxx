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

#ifndef MPD_PID_FILE_HXX
#define MPD_PID_FILE_HXX

#include "fs/FileSystem.hxx"
#include "fs/AllocatedPath.hxx"
#include "Log.hxx"

#include <assert.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>

class PidFile {
	FILE *file;

public:
	PidFile(const AllocatedPath &path):file(nullptr) {
		if (path.IsNull())
			return;

		file = FOpen(path, "w");
		if (file == nullptr) {
			const std::string utf8 = path.ToUTF8();
			FormatFatalSystemError("Failed to create pid file \"%s\"",
					       path.c_str());
		}
	}

	PidFile(const PidFile &) = delete;

	void Close() {
		if (file == nullptr)
			return;

		fclose(file);
	}

	void Delete(const AllocatedPath &path) {
		if (file == nullptr) {
			assert(path.IsNull());
			return;
		}

		assert(!path.IsNull());

		fclose(file);
		RemoveFile(path);
	}

	void Write(pid_t pid) {
		if (file == nullptr)
			return;

		fprintf(file, "%lu\n", (unsigned long)pid);
		fclose(file);
	}

	void Write() {
		if (file == nullptr)
			return;

		Write(getpid());
	}
};

#endif
