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
}
