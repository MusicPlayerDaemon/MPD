/*
 * Copyright (C) 2003-2013 The Music Player Daemon Project
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

#include <glib.h>
#include <assert.h>

class CommandListBuilder {
	/**
	 * for when in list mode
	 */
	GSList *cmd_list;

	/**
	 * print OK after each command execution
	 */
	int cmd_list_OK;

	/**
	 * mem cmd_list consumes
	 */
	size_t cmd_list_size;

public:
	CommandListBuilder()
		:cmd_list(nullptr), cmd_list_OK(-1), cmd_list_size(0) {}
	~CommandListBuilder() {
		Reset();
	}

	/**
	 * Is a command list currently being built?
	 */
	bool IsActive() const {
		assert(cmd_list_OK >= -1 && cmd_list_OK <= 1);

		return cmd_list_OK >= 0;
	}

	/**
	 * Is the object in "list_OK" mode?
	 */
	bool IsOKMode() const {
		assert(IsActive());

		return (bool)cmd_list_OK;
	}

	/**
	 * Reset the object: delete the list and clear the mode.
	 */
	void Reset();

	/**
	 * Begin building a command list.
	 */
	void Begin(bool ok) {
		assert(cmd_list == nullptr);
		assert(cmd_list_OK == -1);

		cmd_list_OK = (int)ok;
	}

	/**
	 * @return false if the list is full
	 */
	bool Add(const char *cmd);

	/**
	 * Finishes the list and returns it.
	 */
	GSList *Commit() {
		assert(IsActive());

		/* for scalability reasons, we have prepended each new
		   command; now we have to reverse it to restore the
		   correct order */
		return cmd_list = g_slist_reverse(cmd_list);
	}
};

#endif
