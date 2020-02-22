/*
 * Copyright 2015-2018 Cary Audio
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

#include "config.h"
#include "MiscSticker.hxx"
#include "StickerDatabase.hxx"
#include "util/Alloc.hxx"

#include <assert.h>
#include <string.h>
#include <stdlib.h>

static const char *type = "misc";

std::string
sticker_misc_get_value(const char *uri, const char *name)
{
	return sticker_load_value(type, uri, name);
}

void
sticker_misc_set_value(const char *uri,
		       const char *name, const char *value)
{
	sticker_store_value(type, uri, name, value);
}

bool
sticker_misc_delete(const char *uri)
{
	return sticker_delete(type, uri);
}

bool
sticker_misc_delete_value(const char *uri, const char *name)
{
	return sticker_delete_value(type, uri, name);
}

Sticker *
sticker_misc_get(const char *uri)
{
	return sticker_load(type, uri);
}

