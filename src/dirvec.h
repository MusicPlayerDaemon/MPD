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

#ifndef MPD_DIRVEC_H
#define MPD_DIRVEC_H

#include <stddef.h>

struct dirvec {
	struct directory **base;
	size_t nr;
};

void dirvec_init(void);

void dirvec_deinit(void);

void dirvec_sort(struct dirvec *dv);

struct directory *dirvec_find(const struct dirvec *dv, const char *path);

int dirvec_delete(struct dirvec *dv, struct directory *del);

void dirvec_add(struct dirvec *dv, struct directory *add);

static inline void
dirvec_clear(struct dirvec *dv)
{
	dv->nr = 0;
}

void dirvec_destroy(struct dirvec *dv);

int dirvec_for_each(const struct dirvec *dv,
                    int (*fn)(struct directory *, void *), void *arg);

#endif /* DIRVEC_H */
