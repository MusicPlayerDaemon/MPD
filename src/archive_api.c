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

#include "config.h" /* must be first for large file support */
#include "archive_api.h"

#include <stdio.h>

#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <glib.h>

/**
 *
 * archive_lookup is used to determine if part of pathname refers to an regular
 * file (archive). If so then its also used to split pathname into archive file
 * and path used to locate file in archive. It also returns suffix of the file.
 * How it works:
 * We do stat of the parent of input pathname as long as we find an regular file
 * Normally this should never happen. When routine returns true pathname modified
 * and split into archive, inpath and suffix. Otherwise nothing happens
 *
 * For example:
 *
 * /music/path/Talco.zip/Talco - Combat Circus/12 - A la pachenka.mp3
 * is split into archive:	/music/path/Talco.zip
 * inarchive pathname:		Talco - Combat Circus/12 - A la pachenka.mp3
 * and suffix:			 zip
 */

bool archive_lookup(char *pathname, char **archive, char **inpath, char **suffix)
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
				g_warning("stat %s failed (errno=%d)\n", pathdupe, errno);
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
				*suffix = NULL;
				while (idx > 0) {
					if (pathname[idx] == '.') {
						*suffix = pathname + idx + 1;
						break;
					}
					idx--;
				}
				break;
			} else {
				g_warning("not a regular file %s\n", pathdupe);
				break;
			}
		}
		//find one dir up
		while (idx > 0) {
			if (pathdupe[idx] == '/') {
				pathdupe[idx] = 0;
				break;
			}
			idx--;
		}
	}
	g_free(pathdupe);
	return ret;
}

