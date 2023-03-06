// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_DB_ERROR_HXX
#define MPD_DB_ERROR_HXX

#include <stdexcept>

enum class DatabaseErrorCode {
	/**
	 * The database is disabled, i.e. none is configured in this
	 * MPD instance.
	 */
	DISABLED,

	NOT_FOUND,

	CONFLICT,
};

class DatabaseError final : public std::runtime_error {
	DatabaseErrorCode code;

public:
	DatabaseError(DatabaseErrorCode _code, const char *msg)
		:std::runtime_error(msg), code(_code) {}

	DatabaseErrorCode GetCode() const {
		return code;
	}
};

#endif
