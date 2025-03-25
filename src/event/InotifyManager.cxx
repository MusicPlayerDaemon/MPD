// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "InotifyManager.hxx"
#include "util/PrintException.hxx"

#include <cassert>

#include <sys/inotify.h>

bool
InotifyWatch::TryAddWatch(const char *pathname, uint32_t mask) noexcept
{
	assert(!IsWatching());

	if (manager.IsShuttingDown()) [[unlikely]]
		/* ignore silently */
		return true;

	oneshot = (mask & IN_ONESHOT) != 0;
	watch_descriptor = manager.event.TryAddWatch(pathname, mask);
	const bool success = IsWatching();
	if (success)
		manager.watches.insert(*this);

	return success;
}

void
InotifyWatch::AddWatch(const char *pathname, uint32_t mask)
{
	assert(!IsWatching());

	if (manager.IsShuttingDown()) [[unlikely]]
		/* ignore silently */
		return;

	oneshot = (mask & IN_ONESHOT) != 0;
	watch_descriptor = manager.event.AddWatch(pathname, mask);
	assert(IsWatching());

	manager.watches.insert(*this);
}

void
InotifyWatch::RemoveWatch() noexcept
{
	if (!IsWatching())
		return;

	assert(!manager.IsShuttingDown());

	manager.watches.erase(manager.watches.iterator_to(*this));
	manager.event.RemoveWatch(std::exchange(watch_descriptor, -1));
}

InotifyManager::InotifyManager(EventLoop &event_loop)
	:event(event_loop, *this)
{
}

InotifyManager::~InotifyManager() noexcept
{
	assert(watches.empty());
}

void
InotifyManager::BeginShutdown() noexcept
{
	event.Close();

	watches.clear_and_dispose([](InotifyWatch *w){
		assert(w->IsWatching());

		/* don't bother calling inotify_rm_watch() because the
		   inotify file descriptor has been closed already */
		w->watch_descriptor = -1;
	});
}

void
InotifyManager::OnInotify(int wd, unsigned mask, const char *name)
{
	const auto i = watches.find(wd);
	if (i == watches.end()) [[unlikely]]
		// should not happen
		return;

	if (i->oneshot) {
		watches.erase(i);
		i->watch_descriptor = -1;
	}

	i->OnInotify(mask, name);
}

void
InotifyManager::OnInotifyError(std::exception_ptr error) noexcept
{
	BeginShutdown();

	PrintException(std::move(error));
}
