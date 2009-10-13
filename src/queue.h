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

#ifndef QUEUE_H
#define QUEUE_H

#include <glib.h>

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

enum {
	/**
	 * reserve max_length * QUEUE_HASH_MULT elements in the id
	 * number space
	 */
	QUEUE_HASH_MULT = 4,
};

/**
 * One element of the queue: basically a song plus some queue specific
 * information attached.
 */
struct queue_item {
	struct song *song;

	/** the unique id of this item in the queue */
	unsigned id;

	/** when was this item last changed? */
	uint32_t version;
};

/**
 * A queue of songs.  This is the backend of the playlist: it contains
 * an ordered list of songs.
 *
 * Songs can be addressed in three possible ways:
 *
 * - the position in the queue
 * - the unique id (which stays the same, regardless of moves)
 * - the order number (which only differs from "position" in random mode)
 */
struct queue {
	/** configured maximum length of the queue */
	unsigned max_length;

	/** number of songs in the queue */
	unsigned length;

	/** the current version number */
	uint32_t version;

	/** all songs in "position" order */
	struct queue_item *items;

	/** map order numbers to positions */
	unsigned *order;

	/** map song ids to positions */
	int *id_to_position;

	/** repeat playback when the end of the queue has been
	    reached? */
	bool repeat;

	/** play only current song. */
	bool single;

	/** remove each played files. */
	bool consume;

	/** play back songs in random order? */
	bool random;

	/** random number generator for shuffle and random mode */
	GRand *rand;
};

static inline unsigned
queue_length(const struct queue *queue)
{
	assert(queue->length <= queue->max_length);

	return queue->length;
}

/**
 * Determine if the queue is empty, i.e. there are no songs.
 */
static inline bool
queue_is_empty(const struct queue *queue)
{
	return queue->length == 0;
}

/**
 * Determine if the maximum number of songs has been reached.
 */
static inline bool
queue_is_full(const struct queue *queue)
{
	assert(queue->length <= queue->max_length);

	return queue->length >= queue->max_length;
}

/**
 * Is that a valid position number?
 */
static inline bool
queue_valid_position(const struct queue *queue, unsigned position)
{
	return position < queue->length;
}

/**
 * Is that a valid order number?
 */
static inline bool
queue_valid_order(const struct queue *queue, unsigned order)
{
	return order < queue->length;
}

static inline int
queue_id_to_position(const struct queue *queue, unsigned id)
{
	if (id >= queue->max_length * QUEUE_HASH_MULT)
		return -1;

	assert(queue->id_to_position[id] >= -1);
	assert(queue->id_to_position[id] < (int)queue->length);

	return queue->id_to_position[id];
}

static inline int
queue_position_to_id(const struct queue *queue, unsigned position)
{
	assert(position < queue->length);

	return queue->items[position].id;
}

static inline unsigned
queue_order_to_position(const struct queue *queue, unsigned order)
{
	assert(order < queue->length);

	return queue->order[order];
}

static inline unsigned
queue_position_to_order(const struct queue *queue, unsigned position)
{
	assert(position < queue->length);

	for (unsigned i = 0;; ++i) {
		assert(i < queue->length);

		if (queue->order[i] == position)
			return i;
	}
}

/**
 * Returns the song at the specified position.
 */
static inline struct song *
queue_get(const struct queue *queue, unsigned position)
{
	assert(position < queue->length);

	return queue->items[position].song;
}

/**
 * Returns the song at the specified order number.
 */
static inline struct song *
queue_get_order(const struct queue *queue, unsigned order)
{
	return queue_get(queue, queue_order_to_position(queue, order));
}

/**
 * Is the song at the specified position newer than the specified
 * version?
 */
static inline bool
queue_song_newer(const struct queue *queue, unsigned position,
		 uint32_t version)
{
	assert(position < queue->length);

	return version > queue->version ||
		queue->items[position].version >= version ||
		queue->items[position].version == 0;
}

/**
 * Initialize a queue object.
 */
void
queue_init(struct queue *queue, unsigned max_length);

/**
 * Deinitializes a queue object.  It does not free the queue pointer
 * itself.
 */
void
queue_finish(struct queue *queue);

/**
 * Returns the order number following the specified one.  This takes
 * end of queue and "repeat" mode into account.
 *
 * @return the next order number, or -1 to stop playback
 */
int
queue_next_order(const struct queue *queue, unsigned order);

/**
 * Increments the queue's version number.  This handles integer
 * overflow well.
 */
void
queue_increment_version(struct queue *queue);

/**
 * Marks the specified song as "modified" and increments the version
 * number.
 */
void
queue_modify(struct queue *queue, unsigned order);

/**
 * Marks all songs as "modified" and increments the version number.
 */
void
queue_modify_all(struct queue *queue);

/**
 * Appends a song to the queue and returns its position.  Prior to
 * that, the caller must check if the queue is already full.
 *
 * If a song is not in the database (determined by
 * song_in_database()), it is freed when removed from the queue.
 */
unsigned
queue_append(struct queue *queue, struct song *song);

/**
 * Swaps two songs, addressed by their position.
 */
void
queue_swap(struct queue *queue, unsigned position1, unsigned position2);

/**
 * Swaps two songs, addressed by their order number.
 */
static inline void
queue_swap_order(struct queue *queue, unsigned order1, unsigned order2)
{
	unsigned tmp = queue->order[order1];
	queue->order[order1] = queue->order[order2];
	queue->order[order2] = tmp;
}

/**
 * Moves a song to a new position.
 */
void
queue_move(struct queue *queue, unsigned from, unsigned to);

/**
 * Moves a range of songs to a new position.
 */
void
queue_move_range(struct queue *queue, unsigned start, unsigned end, unsigned to);

/**
 * Removes a song from the playlist.
 */
void
queue_delete(struct queue *queue, unsigned position);

/**
 * Removes all songs from the playlist.
 */
void
queue_clear(struct queue *queue);

/**
 * Initializes the "order" array, and restores "normal" order.
 */
static inline void
queue_restore_order(struct queue *queue)
{
	for (unsigned i = 0; i < queue->length; ++i)
		queue->order[i] = i;
}

/**
 * Shuffles the virtual order of songs, but does not move them
 * physically.  This is used in random mode.
 */
void
queue_shuffle_order(struct queue *queue);

/**
 * Shuffles the virtual order of the last song in the specified
 * (order) range.  This is used in random mode after a song has been
 * appended by queue_append().
 */
void
queue_shuffle_order_last(struct queue *queue, unsigned start, unsigned end);

/**
 * Shuffles a (position) range in the queue.  The songs are physically
 * shuffled, not by using the "order" mapping.
 */
void
queue_shuffle_range(struct queue *queue, unsigned start, unsigned end);

#endif
