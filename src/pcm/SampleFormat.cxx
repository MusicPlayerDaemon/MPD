// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "SampleFormat.hxx"

#include <cassert>

const char *
sample_format_to_string(SampleFormat format) noexcept
{
	switch (format) {
	case SampleFormat::UNDEFINED:
		return "?";

	case SampleFormat::S8:
		return "8";

	case SampleFormat::S16:
		return "16";

	case SampleFormat::S24_P32:
		return "24";

	case SampleFormat::S32:
		return "32";

	case SampleFormat::FLOAT:
		return "f";

	case SampleFormat::DSD:
		return "dsd";
	}

	/* unreachable */
	assert(false);
	gcc_unreachable();
}
