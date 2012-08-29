/*
 * Copyright (C) 2003-2012 The Music Player Daemon Project
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

struct client;
struct queue;
class SongFilter;

void
queue_print_info(struct client *client, const struct queue *queue,
		 unsigned start, unsigned end);

void
queue_print_uris(struct client *client, const struct queue *queue,
		 unsigned start, unsigned end);

void
queue_print_changes_info(struct client *client, const struct queue *queue,
			 uint32_t version);

void
queue_print_changes_position(struct client *client, const struct queue *queue,
			     uint32_t version);

void
queue_find(struct client *client, const struct queue *queue,
	   const SongFilter &filter);

#endif
