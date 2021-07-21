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

#include "Client.hxx"
#include "Partition.hxx"
#include "IdleFlags.hxx"

#include <cassert>

Client::SubscribeResult
Client::Subscribe(const char *channel) noexcept
{
	assert(channel != nullptr);

	if (!client_message_valid_channel_name(channel))
		return Client::SubscribeResult::INVALID;

	if (num_subscriptions >= MAX_SUBSCRIPTIONS)
		return Client::SubscribeResult::FULL;

	if (!subscriptions.insert(channel).second)
		return Client::SubscribeResult::ALREADY;

	++num_subscriptions;

	partition->EmitIdle(IDLE_SUBSCRIPTION);

	return Client::SubscribeResult::OK;
}

bool
Client::Unsubscribe(const char *channel) noexcept
{
	const auto i = subscriptions.find(channel);
	if (i == subscriptions.end())
		return false;

	assert(num_subscriptions > 0);

	subscriptions.erase(i);
	--num_subscriptions;

	partition->EmitIdle(IDLE_SUBSCRIPTION);

	assert((num_subscriptions == 0) ==
	       subscriptions.empty());

	return true;
}

void
Client::UnsubscribeAll() noexcept
{
	subscriptions.clear();
	num_subscriptions = 0;
}

bool
Client::PushMessage(const ClientMessage &msg) noexcept
{
	if (messages.size() >= MAX_MESSAGES ||
	    !IsSubscribed(msg.GetChannel()))
		return false;

	if (messages.empty())
		IdleAdd(IDLE_MESSAGE);

	messages.push_back(msg);
	return true;
}
