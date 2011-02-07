/*
 * Copyright (C) 2003-2010 The Music Player Daemon Project
 * Copyright (C) 2010-2011 Philipp 'ph3-der-loewe' Schafft
 * Copyright (C) 2010-2011 Hans-Kristian 'maister' Arntzen
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


#ifndef __ROAR_OUTPUT_H
#define __ROAR_OUTPUT_H

#include <roaraudio.h>
#include <glib.h>

typedef struct roar
{
	roar_vs_t * vss;
	int err;
	char *host;
	char *name;
	int role;
	struct roar_connection con;
	struct roar_audio_info info;
	GMutex *lock;
	volatile bool alive;
} roar_t;

#endif
