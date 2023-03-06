// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Migrate.hxx"
#include "Data.hxx"
#include "Block.hxx"

static void
MigrateParamToBlockParam(ConfigData &config, ConfigOption old_option,
			 ConfigBlockOption new_block_option,
			 const char *block_id_key, const char *block_id_value,
			 const char *block_value_key) noexcept
{
	const auto *param = config.GetParam(old_option);
	if (param == nullptr)
		return;

	auto &block = config.MakeBlock(new_block_option,
				       block_id_key, block_id_value);
	if (block.GetBlockParam(block_value_key) == nullptr)
		block.AddBlockParam(block_value_key, param->value,
				     param->line);
}

static void
MigrateCurlProxyConfig(ConfigData &config) noexcept
{
	MigrateParamToBlockParam(config, ConfigOption::HTTP_PROXY_HOST,
				 ConfigBlockOption::INPUT, "plugin", "curl",
				 "proxy");
	MigrateParamToBlockParam(config, ConfigOption::HTTP_PROXY_PORT,
				 ConfigBlockOption::INPUT, "plugin", "curl",
				 "proxy_port");
	MigrateParamToBlockParam(config, ConfigOption::HTTP_PROXY_USER,
				 ConfigBlockOption::INPUT, "plugin", "curl",
				 "proxy_user");
	MigrateParamToBlockParam(config, ConfigOption::HTTP_PROXY_PASSWORD,
				 ConfigBlockOption::INPUT, "plugin", "curl",
				 "proxy_password");
}

void
Migrate(ConfigData &config) noexcept
{
	MigrateCurlProxyConfig(config);
}
