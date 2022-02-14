/*
 * Copyright 2003-2021 The Music Player Daemon Project
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

#include "util/Compiler.h"
#include "IdTable.hxx"
#include "SingleMode.hxx"
#include "util/LazyRandomEngine.hxx"

#include <cassert>
#include <cstdint>
#include <utility>

struct LightSong;
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
	const unsigned max_length;

	/** number of songs in the queue */
	unsigned length = 0;

	/** the current version number */
	uint32_t version = 1;

	/** all songs in "position" order */
	Item *const items;

	/** map order numbers to positions */
	unsigned *const order;

	/** map song ids to positions */
	IdTable id_table;

	/** repeat playback when the end of the queue has been
	    reached? */
	bool repeat = false;

	/** play only current song. */
	SingleMode single = SingleMode::OFF;

	/** remove each played files. */
	bool consume = false;

	/** play back songs in random order? */
	bool random = false;

	/** random number generator for shuffle and random mode */
	LazyRandomEngine rand;

	explicit Queue(unsigned max_length) noexcept;
	~Queue() noexcept;

	Queue(const Queue &) = delete;
	Queue &operator=(const Queue &) = delete;

	unsigned GetLength() const noexcept {
		assert(length <= max_length);

		return length;
	}

	/**
	 * Determine if the queue is empty, i.e. there are no songs.
	 */
	bool IsEmpty() const noexcept {
		return length == 0;
	}

	/**
	 * Determine if the maximum number of songs has been reached.
	 */
	bool IsFull() const noexcept {
		assert(length <= max_length);

		return length >= max_length;
	}

	/**
	 * Is that a valid position number?
	 */
	bool IsValidPosition(unsigned position) const noexcept {
		return position < length;
	}

	/**
	 * Is that a valid order number?
	 */
	bool IsValidOrder(unsigned _order) const noexcept {
		return _order < length;
	}

	int IdToPosition(unsigned id) const noexcept {
		return id_table.IdToPosition(id);
	}

	int PositionToId(unsigned position) const noexcept {
		assert(position < length);

		return items[position].id;
	}

	gcc_pure
	unsigned OrderToPosition(unsigned _order) const noexcept {
		assert(_order < length);

		return order[_order];
	}

	gcc_pure
	unsigned PositionToOrder(unsigned position) const noexcept {
		assert(position < length);

		for (unsigned i = 0;; ++i) {
			assert(i < length);

			if (order[i] == position)
				return i;
		}
	}

	gcc_pure
	uint8_t GetPriorityAtPosition(unsigned position) const noexcept {
		assert(position < length);

		return items[position].priority;
	}

	const Item &GetOrderItem(unsigned i) const noexcept {
		assert(IsValidOrder(i));

		return items[OrderToPosition(i)];
	}

	uint8_t GetOrderPriority(unsigned i) const noexcept {
		return GetOrderItem(i).priority;
	}

	/**
	 * Returns the song at the specified position.
	 */
	DetachedSong &Get(unsigned position) const noexcept {
		assert(position < length);

		return *items[position].song;
	}

	/**
	 * Like Get(), but return a #LightSong instance.
	 */
	LightSong GetLight(unsigned position) const noexcept;

	/**
	 * Returns the song at the specified order number.
	 */
	DetachedSong &GetOrder(unsigned _order) const noexcept {
		return Get(OrderToPosition(_order));
	}

	/**
	 * Is the song at the specified position newer than the specified
	 * version?
	 */
	bool IsNewerAtPosition(unsigned position,
			       uint32_t _version) const noexcept {
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
	int GetNextOrder(unsigned order) const noexcept;

	/**
	 * Increments the queue's version number.  This handles integer
	 * overflow well.
	 */
	void IncrementVersion() noexcept;

	/**
	 * Marks the specified song as "modified".  Call
	 * IncrementVersion() after all modifications have been made.
	 * number.
	 */
	void ModifyAtPosition(unsigned position) noexcept {
		assert(position < length);

		items[position].version = version;
	}

	/**
	 * Marks the specified song as "modified".  Call
	 * IncrementVersion() after all modifications have been made.
	 * number.
	 */
	void ModifyAtOrder(unsigned order) noexcept;

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
	unsigned Append(DetachedSong &&song, uint8_t priority) noexcept;

	/**
	 * Swaps two songs, addressed by their position.
	 */
	void SwapPositions(unsigned position1, unsigned position2) noexcept;

	/**
	 * Swaps two songs, addressed by their order number.
	 */
	void SwapOrders(unsigned order1, unsigned order2) noexcept {
		std::swap(order[order1], order[order2]);
	}

	/**
	 * Moves a song to a new position in the "order" list.
	 *
	 * @return to_order
	 */
	unsigned MoveOrder(unsigned from_order, unsigned to_order) noexcept;

	/**
	 * Moves a song to a new position in the "order" list before
	 * the given one.
	 *
	 * @return the new order number of the given "from" song
	 */
	unsigned MoveOrderBefore(unsigned from_order,
				 unsigned to_order) noexcept;

	/**
	 * Moves a song to a new position in the "order" list after
	 * the given one.
	 *
	 * @return the new order number of the given "from" song
	 */
	unsigned MoveOrderAfter(unsigned from_order,
				unsigned to_order) noexcept;

	/**
	 * Moves a song to a new position.
	 */
	void MovePostion(unsigned from, unsigned to) noexcept;

	/**
	 * Moves a range of songs to a new position.
	 */
	void MoveRange(unsigned start, unsigned end, unsigned to) noexcept;

	/**
	 * Removes a song from the playlist.
	 */
	void DeletePosition(unsigned position) noexcept;

	/**
	 * Removes all songs from the playlist.
	 */
	void Clear() noexcept;

	/**
	 * Initializes the "order" array, and restores "normal" order.
	 */
	void RestoreOrder() noexcept {
		for (unsigned i = 0; i < length; ++i)
			order[i] = i;
	}

	/**
	 * Shuffle the order of items in the specified range, ignoring
	 * their priorities.
	 */
	void ShuffleOrderRange(unsigned start, unsigned end) noexcept;

	/**
	 * Shuffle the order of items in the specified range, taking their
	 * priorities into account.
	 */
	void ShuffleOrderRangeWithPriority(unsigned start,
					   unsigned end) noexcept;

	/**
	 * Shuffles the virtual order of songs, but does not move them
	 * physically.  This is used in random mode.
	 */
	void ShuffleOrder() noexcept;

	void ShuffleOrderFirst(unsigned start, unsigned end) noexcept;

	/**
	 * Shuffles the virtual order of the last song in the
	 * specified (order) range; only songs which match this song's
	 * priority are considered.  This is used in random mode after
	 * a song has been appended by Append().
	 */
	void ShuffleOrderLastWithPriority(unsigned start, unsigned end) noexcept;

	/**
	 * Shuffles a (position) range in the queue.  The songs are physically
	 * shuffled, not by using the "order" mapping.
	 */
	void ShuffleRange(unsigned start, unsigned end) noexcept;

	bool SetPriority(unsigned position, uint8_t priority, int after_order,
			 bool reorder=true) noexcept;

	bool SetPriorityRange(unsigned start_position, unsigned end_position,
			      uint8_t priority, int after_order) noexcept;

private:
	void MoveItemTo(unsigned from, unsigned to) noexcept {
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
				   unsigned exclude_order) const noexcept;

	gcc_pure
	unsigned CountSamePriority(unsigned start_order,
				   uint8_t priority) const noexcept;
};

#endif
