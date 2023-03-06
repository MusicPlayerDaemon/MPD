// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

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
