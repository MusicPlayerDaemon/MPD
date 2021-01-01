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
