// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

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
