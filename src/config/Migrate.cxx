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
