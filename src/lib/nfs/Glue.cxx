// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Glue.hxx"
#include "Manager.hxx"
#include "event/Call.hxx"
#include "util/Manual.hxx"

#include <cassert>

static Manual<NfsManager> nfs_glue;
static unsigned in_use;

void
nfs_init(EventLoop &event_loop)
{
	if (in_use++ > 0)
		return;

	nfs_glue.Construct(event_loop);
}

void
nfs_finish() noexcept
{
	assert(in_use > 0);

	if (--in_use > 0)
		return;

	BlockingCall(nfs_glue->GetEventLoop(), [](){ nfs_glue.Destruct(); });
}

EventLoop &
nfs_get_event_loop() noexcept
{
	assert(in_use > 0);

	return nfs_glue->GetEventLoop();
}

NfsConnection &
nfs_get_connection(NfsContext &&context, const struct nfs_url &url,
		   bool lookup_existing)
{
	assert(in_use > 0);

	return nfs_glue->GetConnection(std::move(context), url, lookup_existing);
}
