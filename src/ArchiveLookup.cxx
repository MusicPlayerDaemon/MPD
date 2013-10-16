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

#include "config.h" /* must be first for large file support */
#include "ArchiveLookup.hxx"
#include "ArchiveDomain.hxx"
#include "Log.hxx"

#include <glib.h>

#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

gcc_pure
static char *
FindSlash(char *p, size_t i)
{
	for (; i > 0; --i)
		if (p[i] == '/')
			return p + i;

	return nullptr;
}

gcc_pure
static const char *
FindSuffix(const char *p, size_t i)
{
	for (; i > 0; --i) {
		if (p[i] == '.')
			return p + i + 1;
	}

	return nullptr;
}

bool
archive_lookup(char *pathname, const char **archive,
	       const char **inpath, const char **suffix)
{
	char *pathdupe;
	int len, idx;
	struct stat st_info;
	bool ret = false;

	*archive = NULL;
	*inpath = NULL;
	*suffix = NULL;

	pathdupe = g_strdup(pathname);
	len = idx = strlen(pathname);

	while (idx > 0) {
		//try to stat if its real directory
		if (stat(pathdupe, &st_info) == -1) {
			if (errno != ENOTDIR) {
				FormatErrno(archive_domain,
					    "Failed to stat %s", pathdupe);
				break;
			}
		} else {
			//is something found ins original path (is not an archive)
			if (idx == len) {
				break;
			}
			//its a file ?
			if (S_ISREG(st_info.st_mode)) {
				//so the upper should be file
				pathname[idx] = 0;
				ret = true;
				*archive = pathname;
				*inpath = pathname + idx+1;

				//try to get suffix
				*suffix = FindSuffix(pathname, idx);
				break;
			} else {
				FormatError(archive_domain,
					    "Not a regular file: %s",
					    pathdupe);
				break;
			}
		}

		//find one dir up
		char *slash = FindSlash(pathdupe, idx);
		if (slash == nullptr)
			break;

		*slash = 0;
		idx = slash - pathdupe;
	}
	g_free(pathdupe);
	return ret;
}

