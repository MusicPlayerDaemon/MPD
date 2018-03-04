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

#ifndef MPD_UPNP_TAGS_HXX
#define MPD_UPNP_TAGS_HXX

struct mime_table {
	const char *suffix;
	const char *mime_name;
};

/**
 * Map UPnP property names to MPD tags.
 */
extern const struct tag_table upnp_tags[];

/**
 * Map UPnP audio file mime type and suffix
 */
extern const struct mime_table mime_types[];


const char *
mime_table_lookup(const mime_table *table, const char *mime);


#endif
