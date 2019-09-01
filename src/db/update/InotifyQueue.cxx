/*
 * Copyright 2003-2019 The Music Player Daemon Project
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

#include "InotifyQueue.hxx"
#include "InotifyDomain.hxx"
#include "Service.hxx"
#include "Log.hxx"
#include "protocol/Ack.hxx" // for class ProtocolError
#include "util/StringCompare.hxx"

/**
 * Wait this long after the last change before calling
 * UpdateService::Enqueue().  This increases the probability that
 * updates can be bundled.
 */
static constexpr std::chrono::steady_clock::duration INOTIFY_UPDATE_DELAY =
	std::chrono::seconds(5);

void
InotifyQueue::OnDelay() noexcept
{
	unsigned id;

	while (!queue.empty()) {
		const char *uri_utf8 = queue.front().c_str();

		try {
			try {
				id = update.Enqueue(uri_utf8, false);
			} catch (const ProtocolError &e) {
				if (e.GetCode() == ACK_ERROR_UPDATE_ALREADY) {
					/* retry later */
					delay_event.Schedule(INOTIFY_UPDATE_DELAY);
					return;
				}

				throw;
			}
		} catch (...) {
			FormatError(std::current_exception(),
				    "Failed to enqueue '%s'", uri_utf8);
			queue.pop_front();
			continue;
		}

		FormatDebug(inotify_domain, "updating '%s' job=%u",
			    uri_utf8, id);

		queue.pop_front();
	}
}

gcc_pure
static bool
path_in(const char *path, const char *possible_parent) noexcept
{
	if (StringIsEmpty(path))
		return true;

	auto rest = StringAfterPrefix(path, possible_parent);
	return rest != nullptr &&
		(StringIsEmpty(rest) || rest[0] == '/');
}

void
InotifyQueue::Enqueue(const char *uri_utf8) noexcept
{
	delay_event.Schedule(INOTIFY_UPDATE_DELAY);

	for (auto i = queue.begin(), end = queue.end(); i != end;) {
		const char *current_uri = i->c_str();

		if (path_in(uri_utf8, current_uri))
			/* already enqueued */
			return;

		if (path_in(current_uri, uri_utf8))
			/* existing path is a sub-path of the new
			   path; we can dequeue the existing path and
			   update the new path instead */
			i = queue.erase(i);
		else
			++i;
	}

	queue.emplace_back(uri_utf8);
}
