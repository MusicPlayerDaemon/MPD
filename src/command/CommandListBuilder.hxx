// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

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
