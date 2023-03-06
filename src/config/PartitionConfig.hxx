// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include "QueueConfig.hxx"
#include "PlayerConfig.hxx"

struct PartitionConfig {
	QueueConfig queue;
	PlayerConfig player;

	PartitionConfig() = default;

	explicit PartitionConfig(const ConfigData &config);
};
