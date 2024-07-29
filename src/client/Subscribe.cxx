// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Client.hxx"
#include "Partition.hxx"
#include "protocol/IdleFlags.hxx"

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
