/*
 * Copyright (C) 2003-2011 The Music Player Daemon Project
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

/**
 * "struct strset" is a hashed string set: you can add strings to this
 * library, and it stores them as a set of unique strings.  You can
 * get the size of the set, and you can enumerate through all values.
 *
 * It is important to note that the strset does not copy the string
 * values - it stores the exact pointers it was given in strset_add().
 */

#ifndef MPD_STRSET_H
#define MPD_STRSET_H

#include <glib.h>

struct strset;

G_GNUC_MALLOC struct strset *strset_new(void);

void strset_free(struct strset *set);

void strset_add(struct strset *set, const char *value);

int strset_get(const struct strset *set, const char *value);

unsigned strset_size(const struct strset *set);

void strset_rewind(struct strset *set);

const char *strset_next(struct strset *set);

#endif
