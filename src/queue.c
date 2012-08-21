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

#include "config.h"
#include "queue.h"
#include "song.h"

#include <stdlib.h>

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

	if (queue->single && queue->repeat && !queue->consume)
		return order;
	else if (order + 1 < queue->length)
		return order + 1;
	else if (queue->repeat && (order > 0 || !queue->consume))
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
queue_append(struct queue *queue, struct song *song, uint8_t priority)
{
	unsigned id = queue_generate_id(queue);

	assert(!queue_is_full(queue));

	queue->items[queue->length] = (struct queue_item){
		.song = song,
		.id = id,
		.version = queue->version,
		.priority = priority,
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

/**
 * Moves a song to a new position in the "order" list.
 */
static void
queue_move_order(struct queue *queue, unsigned from_order, unsigned to_order)
{
	assert(queue != NULL);
	assert(from_order < queue->length);
	assert(to_order <= queue->length);

	const unsigned from_position =
		queue_order_to_position(queue, from_order);

	if (from_order < to_order) {
		for (unsigned i = from_order; i < to_order; ++i)
			queue->order[i] = queue->order[i + 1];
	} else {
		for (unsigned i = from_order; i > to_order; --i)
			queue->order[i] = queue->order[i - 1];
	}

	queue->order[to_order] = from_position;
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

static const struct queue_item *
queue_get_order_item_const(const struct queue *queue, unsigned order)
{
	assert(queue != NULL);
	assert(order < queue->length);

	return &queue->items[queue->order[order]];
}

static uint8_t
queue_get_order_priority(const struct queue *queue, unsigned order)
{
	return queue_get_order_item_const(queue, order)->priority;
}

static gint
queue_item_compare_order_priority(gconstpointer av, gconstpointer bv,
				  gpointer user_data)
{
	const struct queue *queue = user_data;
	const unsigned *const ap = av;
	const unsigned *const bp = bv;
	assert(ap >= queue->order && ap < queue->order + queue->length);
	assert(bp >= queue->order && bp < queue->order + queue->length);
	uint8_t a = queue->items[*ap].priority;
	uint8_t b = queue->items[*bp].priority;

	if (G_LIKELY(a == b))
		return 0;
	else if (a > b)
		return -1;
	else
		return 1;
}

static void
queue_sort_order_by_priority(struct queue *queue, unsigned start, unsigned end)
{
	assert(queue != NULL);
	assert(queue->random);
	assert(start <= end);
	assert(end <= queue->length);

	g_qsort_with_data(&queue->order[start], end - start,
			  sizeof(queue->order[0]),
			  queue_item_compare_order_priority,
			  queue);
}

/**
 * Shuffle the order of items in the specified range, ignoring their
 * priorities.
 */
static void
queue_shuffle_order_range(struct queue *queue, unsigned start, unsigned end)
{
	assert(queue != NULL);
	assert(queue->random);
	assert(start <= end);
	assert(end <= queue->length);

	for (unsigned i = start; i < end; ++i)
		queue_swap_order(queue, i,
				 g_rand_int_range(queue->rand, i, end));
}

/**
 * Sort the "order" of items by priority, and then shuffle each
 * priority group.
 */
void
queue_shuffle_order_range_with_priority(struct queue *queue,
					unsigned start, unsigned end)
{
	assert(queue != NULL);
	assert(queue->random);
	assert(start <= end);
	assert(end <= queue->length);

	if (start == end)
		return;

	/* first group the range by priority */
	queue_sort_order_by_priority(queue, start, end);

	/* now shuffle each priority group */
	unsigned group_start = start;
	uint8_t group_priority = queue_get_order_priority(queue, start);

	for (unsigned i = start + 1; i < end; ++i) {
		uint8_t priority = queue_get_order_priority(queue, i);
		assert(priority <= group_priority);

		if (priority != group_priority) {
			/* start of a new group - shuffle the one that
			   has just ended */
			queue_shuffle_order_range(queue, group_start, i);
			group_start = i;
			group_priority = priority;
		}
	}

	/* shuffle the last group */
	queue_shuffle_order_range(queue, group_start, end);
}

void
queue_shuffle_order(struct queue *queue)
{
	queue_shuffle_order_range_with_priority(queue, 0, queue->length);
}

static void
queue_shuffle_order_first(struct queue *queue, unsigned start, unsigned end)
{
	queue_swap_order(queue, start,
			 g_rand_int_range(queue->rand, start, end));
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

/**
 * Find the first item that has this specified priority or higher.
 */
G_GNUC_PURE
static unsigned
queue_find_priority_order(const struct queue *queue, unsigned start_order,
			  uint8_t priority, unsigned exclude_order)
{
	assert(queue != NULL);
	assert(queue->random);
	assert(start_order <= queue->length);

	for (unsigned order = start_order; order < queue->length; ++order) {
		const unsigned position = queue_order_to_position(queue, order);
		const struct queue_item *item = &queue->items[position];
		if (item->priority <= priority && order != exclude_order)
			return order;
	}

	return queue->length;
}

G_GNUC_PURE
static unsigned
queue_count_same_priority(const struct queue *queue, unsigned start_order,
			  uint8_t priority)
{
	assert(queue != NULL);
	assert(queue->random);
	assert(start_order <= queue->length);

	for (unsigned order = start_order; order < queue->length; ++order) {
		const unsigned position = queue_order_to_position(queue, order);
		const struct queue_item *item = &queue->items[position];
		if (item->priority != priority)
			return order - start_order;
	}

	return queue->length - start_order;
}

bool
queue_set_priority(struct queue *queue, unsigned position, uint8_t priority,
		   int after_order)
{
	assert(queue != NULL);
	assert(position < queue->length);

	struct queue_item *item = &queue->items[position];
	uint8_t old_priority = item->priority;
	if (old_priority == priority)
		return false;

	item->version = queue->version;
	item->priority = priority;

	if (!queue->random)
		/* don't reorder if not in random mode */
		return true;

	unsigned order = queue_position_to_order(queue, position);
	if (after_order >= 0) {
		if (order == (unsigned)after_order)
			/* don't reorder the current song */
			return true;

		if (order < (unsigned)after_order) {
			/* the specified song has been played already
			   - enqueue it only if its priority has just
			   become bigger than the current one's */

			const unsigned after_position =
				queue_order_to_position(queue, after_order);
			const struct queue_item *after_item =
				&queue->items[after_position];
			if (old_priority > after_item->priority ||
			    priority <= after_item->priority)
				/* priority hasn't become bigger */
				return true;
		}
	}

	/* move the item to the beginning of the priority group (or
	   create a new priority group) */

	const unsigned before_order =
		queue_find_priority_order(queue, after_order + 1, priority,
					  order);
	const unsigned new_order = before_order > order
		? before_order - 1
		: before_order;
	queue_move_order(queue, order, new_order);

	/* shuffle the song within that priority group */

	const unsigned priority_count =
		queue_count_same_priority(queue, new_order, priority);
	assert(priority_count >= 1);
	queue_shuffle_order_first(queue, new_order,
				  new_order + priority_count);

	return true;
}

bool
queue_set_priority_range(struct queue *queue,
			 unsigned start_position, unsigned end_position,
			 uint8_t priority, int after_order)
{
	assert(queue != NULL);
	assert(start_position <= end_position);
	assert(end_position <= queue->length);

	bool modified = false;
	int after_position = after_order >= 0
		? (int)queue_order_to_position(queue, after_order)
		: -1;
	for (unsigned i = start_position; i < end_position; ++i) {
		after_order = after_position >= 0
			? (int)queue_position_to_order(queue, after_position)
			: -1;

		modified |= queue_set_priority(queue, i, priority,
					       after_order);
	}

	return modified;
}
