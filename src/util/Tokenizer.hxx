/*
 * Copyright (C) 2009-2014 Max Kellermann <max.kellermann@gmail.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef TOKENIZER_HXX
#define TOKENIZER_HXX

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
	 * Reads the next word.  Throws std::runtime_error on error.
	 *
	 * @return a pointer to the null-terminated word, or nullptr
	 * on end of line
	 */
	char *NextWord();

	/**
	 * Reads the next unquoted word from the input string.  Throws
	 * std::runtime_error on error.
	 *
	 * @return a pointer to the null-terminated word, or nullptr
	 * on end of line
	 */
	char *NextUnquoted();

	/**
	 * Reads the next quoted string from the input string.  A
	 * backslash escapes the following character.  This function
	 * modifies the input string.  Throws std::runtime_error on
	 * error.
	 *
	 * @return a pointer to the null-terminated string, or nullptr
	 * end of line
	 */
	char *NextString();

	/**
	 * Reads the next unquoted word or quoted string from the
	 * input.  This is a wrapper for NextUnquoted() and
	 * NextString().  Throws std::runtime_error on error.
	 *
	 * @return a pointer to the null-terminated string, or nullptr
	 * on end of line
	 */
	char *NextParam();
};

#endif
