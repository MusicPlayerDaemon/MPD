/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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

#ifndef MPD_QUEUE_HXX
#define MPD_QUEUE_HXX

#include "Compiler.h"
#include "IdTable.hxx"
#include "util/LazyRandomEngine.hxx"

#include <algorithm>

#include <assert.h>
#include <stdint.h>

class DetachedSong;

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
struct Queue {
	/**
	 * reserve max_length * HASH_MULT elements in the id
	 * number space
	 */
	static constexpr unsigned HASH_MULT = 4;

	/**
	 * One element of the queue: basically a song plus some queue specific
	 * information attached.
	 */
	struct Item {
		DetachedSong *song;

		/** the unique id of this item in the queue */
		unsigned id;

		/** when was this item last changed? */
		uint32_t version;

		/**
		 * The priority of this item, between 0 and 255.  High
		 * priority value means that this song gets played first in
		 * "random" mode.
		 */
		uint8_t priority;
	};

	/** configured maximum length of the queue */
	unsigned max_length;

	/** number of songs in the queue */
	unsigned length;

	/** the current version number */
	uint32_t version;

	/** all songs in "position" order */
	Item *items;

	/** map order numbers to positions */
	unsigned *order;

	/** map song ids to positions */
	IdTable id_table;

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
	LazyRandomEngine rand;

	explicit Queue(unsigned max_length);

	/**
	 * Deinitializes a queue object.  It does not free the queue
	 * pointer itself.
	 */
	~Queue();

	Queue(const Queue &) = delete;
	Queue &operator=(const Queue &) = delete;

	unsigned GetLength() const {
		assert(length <= max_length);

		return length;
	}

	/**
	 * Determine if the queue is empty, i.e. there are no songs.
	 */
	bool IsEmpty() const {
		return length == 0;
	}

	/**
	 * Determine if the maximum number of songs has been reached.
	 */
	bool IsFull() const {
		assert(length <= max_length);

		return length >= max_length;
	}

	/**
	 * Is that a valid position number?
	 */
	bool IsValidPosition(unsigned position) const {
		return position < length;
	}

	/**
	 * Is that a valid order number?
	 */
	bool IsValidOrder(unsigned _order) const {
		return _order < length;
	}

	int IdToPosition(unsigned id) const {
		return id_table.IdToPosition(id);
	}

	int PositionToId(unsigned position) const
	{
		assert(position < length);

		return items[position].id;
	}

	gcc_pure
	unsigned OrderToPosition(unsigned _order) const {
		assert(_order < length);

		return order[_order];
	}

	gcc_pure
	unsigned PositionToOrder(unsigned position) const {
		assert(position < length);

		for (unsigned i = 0;; ++i) {
			assert(i < length);

			if (order[i] == position)
				return i;
		}
	}

	gcc_pure
	uint8_t GetPriorityAtPosition(unsigned position) const {
		assert(position < length);

		return items[position].priority;
	}

	const Item &GetOrderItem(unsigned i) const {
		assert(IsValidOrder(i));

		return items[OrderToPosition(i)];
	}

	uint8_t GetOrderPriority(unsigned i) const {
		return GetOrderItem(i).priority;
	}

	/**
	 * Returns the song at the specified position.
	 */
	DetachedSong &Get(unsigned position) const {
		assert(position < length);

		return *items[position].song;
	}

	/**
	 * Returns the song at the specified order number.
	 */
	DetachedSong &GetOrder(unsigned _order) const {
		return Get(OrderToPosition(_order));
	}

	/**
	 * Is the song at the specified position newer than the specified
	 * version?
	 */
	bool IsNewerAtPosition(unsigned position, uint32_t _version) const {
		assert(position < length);

		return _version > version ||
			items[position].version >= _version ||
			items[position].version == 0;
	}

	/**
	 * Returns the order number following the specified one.  This takes
	 * end of queue and "repeat" mode into account.
	 *
	 * @return the next order number, or -1 to stop playback
	 */
	gcc_pure
	int GetNextOrder(unsigned order) const;

	/**
	 * Increments the queue's version number.  This handles integer
	 * overflow well.
	 */
	void IncrementVersion();

	/**
	 * Marks the specified song as "modified".  Call
	 * IncrementVersion() after all modifications have been made.
	 * number.
	 */
	void ModifyAtPosition(unsigned position) {
		assert(position < length);

		items[position].version = version;
	}

	/**
	 * Marks the specified song as "modified".  Call
	 * IncrementVersion() after all modifications have been made.
	 * number.
	 */
	void ModifyAtOrder(unsigned order);

	/**
	 * Appends a song to the queue and returns its position.  Prior to
	 * that, the caller must check if the queue is already full.
	 *
	 * If a song is not in the database (determined by
	 * Song::IsInDatabase()), it is freed when removed from the
	 * queue.
	 *
	 * @param priority the priority of this new queue item
	 */
	unsigned Append(DetachedSong &&song, uint8_t priority);

	/**
	 * Swaps two songs, addressed by their position.
	 */
	void SwapPositions(unsigned position1, unsigned position2);

	/**
	 * Swaps two songs, addressed by their order number.
	 */
	void SwapOrders(unsigned order1, unsigned order2) {
		std::swap(order[order1], order[order2]);
	}

	/**
	 * Moves a song to a new position.
	 */
	void MovePostion(unsigned from, unsigned to);

	/**
	 * Moves a range of songs to a new position.
	 */
	void MoveRange(unsigned start, unsigned end, unsigned to);

	/**
	 * Removes a song from the playlist.
	 */
	void DeletePosition(unsigned position);

	/**
	 * Removes all songs from the playlist.
	 */
	void Clear();

	/**
	 * Initializes the "order" array, and restores "normal" order.
	 */
	void RestoreOrder() {
		for (unsigned i = 0; i < length; ++i)
			order[i] = i;
	}

	/**
	 * Shuffle the order of items in the specified range, ignoring
	 * their priorities.
	 */
	void ShuffleOrderRange(unsigned start, unsigned end);

	/**
	 * Shuffle the order of items in the specified range, taking their
	 * priorities into account.
	 */
	void ShuffleOrderRangeWithPriority(unsigned start, unsigned end);

	/**
	 * Shuffles the virtual order of songs, but does not move them
	 * physically.  This is used in random mode.
	 */
	void ShuffleOrder();

	void ShuffleOrderFirst(unsigned start, unsigned end);

	/**
	 * Shuffles the virtual order of the last song in the specified
	 * (order) range.  This is used in random mode after a song has been
	 * appended by queue_append().
	 */
	void ShuffleOrderLast(unsigned start, unsigned end);

	/**
	 * Shuffles a (position) range in the queue.  The songs are physically
	 * shuffled, not by using the "order" mapping.
	 */
	void ShuffleRange(unsigned start, unsigned end);

	bool SetPriority(unsigned position, uint8_t priority, int after_order);

	bool SetPriorityRange(unsigned start_position, unsigned end_position,
			      uint8_t priority, int after_order);

private:
	/**
	 * Moves a song to a new position in the "order" list.
	 */
	void MoveOrder(unsigned from_order, unsigned to_order);

	void MoveItemTo(unsigned from, unsigned to) {
		unsigned from_id = items[from].id;

		items[to] = items[from];
		items[to].version = version;
		id_table.Move(from_id, to);
	}

	/**
	 * Find the first item that has this specified priority or
	 * higher.
	 */
	gcc_pure
	unsigned FindPriorityOrder(unsigned start_order, uint8_t priority,
				   unsigned exclude_order) const;

	gcc_pure
	unsigned CountSamePriority(unsigned start_order,
				   uint8_t priority) const;
};

#endif
