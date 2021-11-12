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

#include "Item.hxx"
#include "Lease.hxx"
#include "input/InputStream.hxx"

#include <cassert>

InputCacheItem::InputCacheItem(InputStreamPtr _input) noexcept
	:BufferingInputStream(std::move(_input)),
	 uri(GetInput().GetURI())
{
}

InputCacheItem::~InputCacheItem() noexcept
{
	assert(leases.empty());
}

void
InputCacheItem::AddLease(InputCacheLease &lease) noexcept
{
	const std::scoped_lock<Mutex> lock(mutex);
	leases.push_back(lease);
}

void
InputCacheItem::RemoveLease(InputCacheLease &lease) noexcept
{
	const std::scoped_lock<Mutex> lock(mutex);
	auto i = leases.iterator_to(lease);
	if (i == next_lease)
		++next_lease;
	leases.erase(i);

	// TODO: ensure that OnBufferAvailable() isn't currently running
}

void
InputCacheItem::OnBufferAvailable() noexcept
{
	for (auto i = leases.begin(); i != leases.end(); i = next_lease) {
		next_lease = std::next(i);
		i->OnInputCacheAvailable();
	}
}
