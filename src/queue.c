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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "queue.h"
#include "song.h"

unsigned
queue_generate_id(const struct queue *queue)
{
	static unsigned cur = (unsigned)-1;

	do {
		cur++;

		if (cur >= queue->max_length * QUEUE_HASH_MULT)
			cur = 0;
	} while (queue->idToPosition[cur] != -1);

	return cur;
}

int
queue_next_order(const struct queue *queue, unsigned order)
{
	assert(order < queue->length);

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
			queue->songMod[i] = 0;

		queue->version = 1;
	}
}

void
queue_modify(struct queue *queue, unsigned order)
{
	unsigned position;

	assert(order < queue->length);

	position = queue->order[order];
	queue->songMod[position] = queue->version;

	queue_increment_version(queue);
}

void
queue_modify_all(struct queue *queue)
{
	for (unsigned i = 0; i < queue->length; i++)
		queue->songMod[i] = queue->version;

	queue_increment_version(queue);
}

unsigned
queue_append(struct queue *queue, struct song *song)
{
	unsigned id = queue_generate_id(queue);

	assert(!queue_is_full(queue));

	queue->songs[queue->length] = song;
	queue->songMod[queue->length] = queue->version;
	queue->order[queue->length] = queue->length;
	queue->positionToId[queue->length] = id;
	queue->idToPosition[queue->positionToId[queue->length]] =
	    queue->length;

	++queue->length;

	return id;
}

void
queue_swap(struct queue *queue, unsigned position1, unsigned position2)
{
	struct song *sTemp;
	unsigned iTemp;

	sTemp = queue->songs[position1];
	queue->songs[position1] = queue->songs[position2];
	queue->songs[position2] = sTemp;

	queue->songMod[position1] = queue->version;
	queue->songMod[position2] = queue->version;

	queue->idToPosition[queue->positionToId[position1]] = position2;
	queue->idToPosition[queue->positionToId[position2]] = position1;

	iTemp = queue->positionToId[position1];
	queue->positionToId[position1] = queue->positionToId[position2];
	queue->positionToId[position2] = iTemp;
}

static void
queue_move_song_to(struct queue *queue, unsigned from, unsigned to)
{
	unsigned from_id = queue->positionToId[from];

	queue->idToPosition[from_id] = to;
	queue->positionToId[to] = from_id;
	queue->songs[to] = queue->songs[from];
	queue->songMod[to] = queue->version;
}

void
queue_move(struct queue *queue, unsigned from, unsigned to)
{
	struct song *song;
	unsigned id;

	song = queue_get(queue, from);
	id = queue_position_to_id(queue, from);

	/* move songs to one less in from->to */

	for (unsigned i = from; i < to; i++)
		queue_move_song_to(queue, i + 1, i);

	/* move songs to one more in to->from */

	for (unsigned i = from; i > to; i--)
		queue_move_song_to(queue, i - 1, i);

	/* put song at _to_ */

	queue->idToPosition[id] = to;
	queue->positionToId[to] = id;
	queue->songs[to] = song;
	queue->songMod[to] = queue->version;
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

	queue->idToPosition[id] = -1;

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
		if (!song_in_database(queue->songs[i]))
			song_free(queue->songs[i]);

		queue->idToPosition[queue->positionToId[i]] = -1;
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

	queue->songs = g_malloc(sizeof(queue->songs[0]) * max_length);
	queue->songMod = g_malloc(sizeof(queue->songMod[0]) *
				    max_length);
	queue->order = g_malloc(sizeof(queue->order[0]) *
				  max_length);
	queue->idToPosition = g_malloc(sizeof(queue->idToPosition[0]) *
				       max_length * QUEUE_HASH_MULT);
	queue->positionToId = g_malloc(sizeof(queue->positionToId[0]) *
				       max_length);

	for (unsigned i = 0; i < max_length * QUEUE_HASH_MULT; ++i)
		queue->idToPosition[i] = -1;

	queue->rand = g_rand_new();
}

void
queue_finish(struct queue *queue)
{
	queue_clear(queue);

	g_free(queue->songs);
	g_free(queue->songMod);
	g_free(queue->order);
	g_free(queue->idToPosition);
	g_free(queue->positionToId);

	g_rand_free(queue->rand);
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
