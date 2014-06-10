/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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

#include "config.h"
#include "ClientList.hxx"
#include "ClientInternal.hxx"

#include <algorithm>

#include <assert.h>

void
ClientList::Remove(Client &client)
{
	assert(!list.empty());

	list.erase(list.iterator_to(client));
}

void
ClientList::CloseAll()
{
	list.clear_and_dispose(Client::Disposer());
}

void
ClientList::IdleAdd(unsigned flags)
{
	assert(flags != 0);

	for (auto &client : list)
		client.IdleAdd(flags);
}
