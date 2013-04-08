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

#ifndef MPD_TOKENIZER_HXX
#define MPD_TOKENIZER_HXX

#include "gerror.h"

class Tokenizer {
	char *input;

public:
	/**
	 * @param _input the input string; the contents will be
	 * modified by this class
	 */
	constexpr Tokenizer(char *_input):input(_input) {}

	Tokenizer(const Tokenizer &) = delete;
	Tokenizer &operator=(const Tokenizer &) = delete;

	char *Rest() {
		return input;
	}

	char CurrentChar() const {
		return *input;
	}

	bool IsEnd() const {
		return CurrentChar() == 0;
	}

	/**
	 * Reads the next word.
	 *
	 * @param error_r if this function returns nullptr and
	 * **input_p!=0, it optionally provides a GError object in
	 * this argument
	 * @return a pointer to the null-terminated word, or nullptr
	 * on error or end of line
	 */
	char *NextWord(GError **error_r);

	/**
	 * Reads the next unquoted word from the input string.
	 *
	 * @param error_r if this function returns nullptr and **input_p!=0, it
	 * optionally provides a GError object in this argument
	 * @return a pointer to the null-terminated word, or nullptr
	 * on error or end of line
	 */
	char *NextUnquoted(GError **error_r);

	/**
	 * Reads the next quoted string from the input string.  A backslash
	 * escapes the following character.  This function modifies the input
	 * string.
	 *
	 * @param input_p the input string; this function returns a pointer to
	 * the first non-whitespace character of the following token
	 * @param error_r if this function returns nullptr and **input_p!=0, it
	 * optionally provides a GError object in this argument
	 * @return a pointer to the null-terminated string, or nullptr on error
	 * or end of line
	 */
	char *NextString(GError **error_r);

	/**
	 * Reads the next unquoted word or quoted string from the
	 * input.  This is a wrapper for NextUnquoted() and
	 * NextString().
	 *
	 * @param error_r if this function returns nullptr and
	 * **input_p!=0, it optionally provides a GError object in
	 * this argument
	 * @return a pointer to the null-terminated string, or nullptr
	 * on error or end of line
	 */
	char *NextParam(GError **error_r);
};

#endif
