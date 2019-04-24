/*
 * Copyright 2003-2019 The Music Player Daemon Project
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

#include "Print.hxx"
#include "Sticker.hxx"
#include "client/Response.hxx"

void
sticker_print_value(Response &r,
		    const char *name, const char *value)
{
	r.Format("sticker: %s=%s\n", name, value);
}

void
sticker_print(Response &r, const Sticker &sticker)
{
	for (const auto &i : sticker.table)
		sticker_print_value(r, i.first.c_str(), i.second.c_str());
}
