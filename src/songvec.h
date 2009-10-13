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

#ifndef MPD_SONGVEC_H
#define MPD_SONGVEC_H

#include <stddef.h>

struct songvec {
	struct song **base;
	size_t nr;
};

void songvec_init(void);

void songvec_deinit(void);

void songvec_sort(struct songvec *sv);

struct song *
songvec_find(const struct songvec *sv, const char *uri);

int
songvec_delete(struct songvec *sv, const struct song *del);

void
songvec_add(struct songvec *sv, struct song *add);

void songvec_destroy(struct songvec *sv);

int
songvec_for_each(const struct songvec *sv,
		 int (*fn)(struct song *, void *), void *arg);

#endif /* SONGVEC_H */
