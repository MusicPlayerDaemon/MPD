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

/*
 * This library sends information about songs in the queue to the
 * client.
 */

#ifndef MPD_QUEUE_PRINT_HXX
#define MPD_QUEUE_PRINT_HXX

#include <stdint.h>

struct Queue;
class SongFilter;
class Response;

void
queue_print_info(Response &r, const Queue &queue,
		 unsigned start, unsigned end);

void
queue_print_uris(Response &r, const Queue &queue,
		 unsigned start, unsigned end);

void
queue_print_changes_info(Response &r, const Queue &queue,
			 uint32_t version,
			 unsigned start, unsigned end);

void
queue_print_changes_position(Response &r, const Queue &queue,
			     uint32_t version,
			     unsigned start, unsigned end);

void
queue_find(Response &response, const Queue &queue,
	   const SongFilter &filter);

#endif
