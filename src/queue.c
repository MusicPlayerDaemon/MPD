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

#include "config.h"
#include "queue.h"
#include "song.h"

/**
 * Generate a non-existing id number.
 */
static unsigned
queue_generate_id(const struct queue *queue)
{
	static unsigned cur = (unsigned)-1;

	do {
		cur++;

		if (cur >= queue->max_length * QUEUE_HASH_MULT)
			cur = 0;
	} while (queue->id_to_position[cur] != -1);

	return cur;
}

int
queue_next_order(const struct queue *queue, unsigned order)
{
	assert(order < queue->length);

	if (queue->single)
	{
		if (queue->repeat)
			return order;
		else
			return -1;
	}
	if (order + 1 < queue->length)
		return order + 1;
	else if (queue->repeat)
		/* restart at first song */
		return 0;
	else
		/* end of queue */
		return -1;
}

void
queue_increment_version(struct queue *queue)
{
	static unsigned long max = ((uint32_t) 1 << 31) - 1;

	queue->version++;

	if (queue->version >= max) {
		for (unsigned i = 0; i < queue->length; i++)
			queue->items[i].version = 0;

		queue->version = 1;
	}
}

void
queue_modify(struct queue *queue, unsigned order)
{
	unsigned position;

	assert(order < queue->length);

	position = queue->order[order];
	queue->items[position].version = queue->version;

	queue_increment_version(queue);
}

void
queue_modify_all(struct queue *queue)
{
	for (unsigned i = 0; i < queue->length; i++)
		queue->items[i].version = queue->version;

	queue_increment_version(queue);
}

unsigned
queue_append(struct queue *queue, struct song *song)
{
	unsigned id = queue_generate_id(queue);

	assert(!queue_is_full(queue));

	queue->items[queue->length] = (struct queue_item){
		.song = song,
		.id = id,
		.version = queue->version,
	};

	queue->order[queue->length] = queue->length;
	queue->id_to_position[id] = queue->length;

	++queue->length;

	return id;
}

void
queue_swap(struct queue *queue, unsigned position1, unsigned position2)
{
	struct queue_item tmp;
	unsigned id1 = queue->items[position1].id;
	unsigned id2 = queue->items[position2].id;

	tmp = queue->items[position1];
	queue->items[position1] = queue->items[position2];
	queue->items[position2] = tmp;

	queue->items[position1].version = queue->version;
	queue->items[position2].version = queue->version;

	queue->id_to_position[id1] = position2;
	queue->id_to_position[id2] = position1;
}

static void
queue_move_song_to(struct queue *queue, unsigned from, unsigned to)
{
	unsigned from_id = queue->items[from].id;

	queue->items[to] = queue->items[from];
	queue->items[to].version = queue->version;
	queue->id_to_position[from_id] = to;
}

void
queue_move(struct queue *queue, unsigned from, unsigned to)
{
	struct queue_item item = queue->items[from];

	/* move songs to one less in from->to */

	for (unsigned i = from; i < to; i++)
		queue_move_song_to(queue, i + 1, i);

	/* move songs to one more in to->from */

	for (unsigned i = from; i > to; i--)
		queue_move_song_to(queue, i - 1, i);

	/* put song at _to_ */

	queue->id_to_position[item.id] = to;
	queue->items[to] = item;
	queue->items[to].version = queue->version;

	/* now deal with order */

	if (queue->random) {
		for (unsigned i = 0; i < queue->length; i++) {
			if (queue->order[i] > from && queue->order[i] <= to)
				queue->order[i]--;
			else if (queue->order[i] < from &&
				 queue->order[i] >= to)
				queue->order[i]++;
			else if (from == queue->order[i])
				queue->order[i] = to;
		}
	}
}

void
queue_move_range(struct queue *queue, unsigned start, unsigned end, unsigned to)
{
	struct queue_item items[end - start];
	// Copy the original block [start,end-1]
	for (unsigned i = start; i < end; i++)
		items[i - start] = queue->items[i];

	// If to > start, we need to move to-start items to start, starting from end
	for (unsigned i = end; i < end + to - start; i++)
		queue_move_song_to(queue, i, start + i - end);

	// If to < start, we need to move start-to items to newend (= end + to - start), starting from to
	// This is the same as moving items from start-1 to to (decreasing), with start-1 going to end-1
	// We have to iterate in this order to avoid writing over something we haven't yet moved
	for (unsigned i = start - 1; i >= to && i != G_MAXUINT; i--)
		queue_move_song_to(queue, i, i + end - start);

	// Copy the original block back in, starting at to.
	for (unsigned i = start; i< end; i++)
	{
		queue->id_to_position[items[i-start].id] = to + i - start;
		queue->items[to + i - start] = items[i-start];
		queue->items[to + i - start].version = queue->version;
	}

	if (queue->random) {
		// Update the positions in the queue.
		// Note that the ranges for these cases are the same as the ranges of
		// the loops above.
		for (unsigned i = 0; i < queue->length; i++) {
			if (queue->order[i] >= end && queue->order[i] < to + end - start)
				queue->order[i] -= end - start;
			else if (queue->order[i] < start &&
				 queue->order[i] >= to)
				queue->order[i] += end - start;
			else if (start <= queue->order[i] && queue->order[i] < end)
				queue->order[i] += to - start;
		}
	}
}

void
queue_delete(struct queue *queue, unsigned position)
{
	struct song *song;
	unsigned id, order;

	assert(position < queue->length);

	song = queue_get(queue, position);
	if (!song_in_database(song))
		song_free(song);

	id = queue_position_to_id(queue, position);
	order = queue_position_to_order(queue, position);

	--queue->length;

	/* release the song id */

	queue->id_to_position[id] = -1;

	/* delete song from songs array */

	for (unsigned i = position; i < queue->length; i++)
		queue_move_song_to(queue, i + 1, i);

	/* delete the entry from the order array */

	for (unsigned i = order; i < queue->length; i++)
		queue->order[i] = queue->order[i + 1];

	/* readjust values in the order array */

	for (unsigned i = 0; i < queue->length; i++)
		if (queue->order[i] > position)
			--queue->order[i];
}

void
queue_clear(struct queue *queue)
{
	for (unsigned i = 0; i < queue->length; i++) {
		struct queue_item *item = &queue->items[i];

		if (!song_in_database(item->song))
			song_free(item->song);

		queue->id_to_position[item->id] = -1;
	}

	queue->length = 0;
}

void
queue_init(struct queue *queue, unsigned max_length)
{
	queue->max_length = max_length;
	queue->length = 0;
	queue->version = 1;
	queue->repeat = false;
	queue->random = false;
	queue->single = false;
	queue->consume = false;

	queue->items = g_new(struct queue_item, max_length);
	queue->order = g_malloc(sizeof(queue->order[0]) *
				  max_length);
	queue->id_to_position = g_malloc(sizeof(queue->id_to_position[0]) *
				       max_length * QUEUE_HASH_MULT);

	for (unsigned i = 0; i < max_length * QUEUE_HASH_MULT; ++i)
		queue->id_to_position[i] = -1;

	queue->rand = g_rand_new();
}

void
queue_finish(struct queue *queue)
{
	queue_clear(queue);

	g_free(queue->items);
	g_free(queue->order);
	g_free(queue->id_to_position);

	g_rand_free(queue->rand);
}

void
queue_shuffle_order(struct queue *queue)
{
	assert(queue->random);

	for (unsigned i = 0; i < queue->length; i++)
		queue_swap_order(queue, i,
				 g_rand_int_range(queue->rand, i,
						  queue->length));
}

void
queue_shuffle_order_last(struct queue *queue, unsigned start, unsigned end)
{
	queue_swap_order(queue, end - 1,
			 g_rand_int_range(queue->rand, start, end));
}

void
queue_shuffle_range(struct queue *queue, unsigned start, unsigned end)
{
	assert(start <= end);
	assert(end <= queue->length);

	for (unsigned i = start; i < end; i++) {
		unsigned ri = g_rand_int_range(queue->rand, i, end);
		queue_swap(queue, i, ri);
	}
}
