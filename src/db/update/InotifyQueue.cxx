// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "InotifyQueue.hxx"
#include "InotifyDomain.hxx"
#include "Service.hxx"
#include "UpdateDomain.hxx"
#include "lib/fmt/ExceptionFormatter.hxx"
#include "protocol/Ack.hxx" // for class ProtocolError
#include "util/StringCompare.hxx"
#include "Log.hxx"

/**
 * Wait this long after the last change before calling
 * UpdateService::Enqueue().  This increases the probability that
 * updates can be bundled.
 */
static constexpr Event::Duration INOTIFY_UPDATE_DELAY =
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
			FmtError(update_domain,
				 "Failed to enqueue {:?}: {}",
				 uri_utf8, std::current_exception());
			queue.pop_front();
			continue;
		}

		FmtDebug(inotify_domain, "updating {:?} job={}",
			 uri_utf8, id);

		queue.pop_front();
	}
}

[[gnu::pure]]
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
