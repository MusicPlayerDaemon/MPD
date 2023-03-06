// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_INPUT_READER_HXX
#define MPD_INPUT_READER_HXX

#include "io/Reader.hxx"

class InputStream;

/**
 * A #Reader implementation which forwards all read calls to
 * InputStream::Read() and logs errors.
 */
class InputStreamReader final : public Reader {
	InputStream &is;

public:
	explicit InputStreamReader(InputStream &_is)
		:is(_is) {}

	/* virtual methods from class Reader */
	size_t Read(void *data, size_t size) override;
};

#endif
