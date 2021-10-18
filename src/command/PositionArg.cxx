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

#include "PositionArg.hxx"
#include "protocol/Ack.hxx"
#include "protocol/ArgParser.hxx"
#include "protocol/RangeArg.hxx"
#include "queue/Playlist.hxx"

static unsigned
RequireCurrentPosition(const playlist &p)
{
	int position = p.GetCurrentPosition();
	if (position < 0)
		throw ProtocolError(ACK_ERROR_PLAYER_SYNC,
				    "No current song");

	return position;
}

unsigned
ParseInsertPosition(const char *s, const playlist &playlist)
{
	const auto queue_length = playlist.queue.GetLength();

	if (*s == '+') {
		/* after the current song */

		const unsigned current = RequireCurrentPosition(playlist);
		assert(current < queue_length);

		return current + 1 +
			ParseCommandArgUnsigned(s + 1,
						queue_length - current - 1);
	} else if (*s == '-') {
		/* before the current song */

		const unsigned current = RequireCurrentPosition(playlist);
		assert(current < queue_length);

		return current - ParseCommandArgUnsigned(s + 1, current);
	} else
		/* absolute position */
		return ParseCommandArgUnsigned(s, queue_length);
}

unsigned
ParseMoveDestination(const char *s, const RangeArg range, const playlist &p)
{
	assert(!range.IsEmpty());
	assert(!range.IsOpenEnded());

	const unsigned queue_length = p.queue.GetLength();

	if (*s == '+') {
		/* after the current song */

		unsigned current = RequireCurrentPosition(p);
		assert(current < queue_length);
		if (range.Contains(current))
			throw ProtocolError(ACK_ERROR_ARG, "Cannot move current song relative to itself");

		if (current >= range.end)
			current -= range.Count();

		return current + 1 +
			ParseCommandArgUnsigned(s + 1,
						queue_length - current - range.Count());
	} else if (*s == '-') {
		/* before the current song */

		unsigned current = RequireCurrentPosition(p);
		assert(current < queue_length);
		if (range.Contains(current))
			throw ProtocolError(ACK_ERROR_ARG, "Cannot move current song relative to itself");

		if (current >= range.end)
			current -= range.Count();

		return current -
			ParseCommandArgUnsigned(s + 1,
						queue_length - current - range.Count());
	} else
		/* absolute position */
		return ParseCommandArgUnsigned(s,
					       queue_length - range.Count());
}
