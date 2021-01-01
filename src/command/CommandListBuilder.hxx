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

#ifndef MPD_COMMAND_LIST_BUILDER_HXX
#define MPD_COMMAND_LIST_BUILDER_HXX

#include <cassert>
#include <list>
#include <string>

class CommandListBuilder {
	/**
	 * print OK after each command execution
	 */
	enum class Mode {
		/**
		 * Not active.
		 */
		DISABLED = -1,

		/**
		 * Enabled in normal list mode.
		 */
		ENABLED = false,

		/**
		 * Enabled in "list_OK" mode.
		 */
		OK = true,
	} mode = Mode::DISABLED;

	/**
	 * for when in list mode
	 */
	std::list<std::string> list;

	/**
	 * Memory consumed by the list.
	 */
	size_t size;

public:
	/**
	 * Is a command list currently being built?
	 */
	bool IsActive() const {
		return mode != Mode::DISABLED;
	}

	/**
	 * Is the object in "list_OK" mode?
	 */
	bool IsOKMode() const {
		assert(IsActive());

		return (bool)mode;
	}

	/**
	 * Reset the object: delete the list and clear the mode.
	 */
	void Reset();

	/**
	 * Begin building a command list.
	 */
	void Begin(bool ok) {
		assert(list.empty());
		assert(mode == Mode::DISABLED);

		mode = (Mode)ok;
		size = 0;
	}

	/**
	 * @return false if the list is full
	 */
	bool Add(const char *cmd);

	/**
	 * Finishes the list and returns it.
	 */
	std::list<std::string> Commit() {
		assert(IsActive());

		return std::move(list);
	}
};

#endif
