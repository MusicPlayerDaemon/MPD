// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

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
