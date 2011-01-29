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

#ifndef MPD_TAG_POOL_H
#define MPD_TAG_POOL_H

#include "tag.h"

#include <glib.h>

extern GMutex *tag_pool_lock;

struct tag_item;

void tag_pool_init(void);

void tag_pool_deinit(void);

struct tag_item *
tag_pool_get_item(enum tag_type type, const char *value, size_t length);

struct tag_item *tag_pool_dup_item(struct tag_item *item);

void tag_pool_put_item(struct tag_item *item);

#endif
