// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "PartitionConfig.hxx"
#include "Data.hxx"

PartitionConfig::PartitionConfig(const ConfigData &config)
	:player(config)
{
	queue.max_length =
		config.GetPositive(ConfigOption::MAX_PLAYLIST_LENGTH,
				   QueueConfig::DEFAULT_MAX_LENGTH);

	if (queue.max_length > QueueConfig::MAX_MAX_LENGTH)
		/* silently clip max_playlist_length to a resonable
		   limit to avoid out-of-memory during startup (or
		   worse, an integer overflow because the allocation
		   size is larger than SIZE_MAX) */
		queue.max_length = QueueConfig::MAX_MAX_LENGTH;
}
