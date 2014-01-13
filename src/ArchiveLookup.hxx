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

#ifndef MPD_ARCHIVE_LOOKUP_HXX
#define MPD_ARCHIVE_LOOKUP_HXX

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
bool
archive_lookup(char *pathname, const char **archive,
	       const char **inpath, const char **suffix);

#endif

