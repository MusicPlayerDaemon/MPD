// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "List.hxx"
#include "util/DeleteDisposer.hxx"

#include <cassert>

ClientList::~ClientList() noexcept
{
	list.clear_and_dispose(DeleteDisposer());
}

void
ClientList::Remove(Client &client) noexcept
{
	assert(!list.empty());

	list.erase(list.iterator_to(client));
}
