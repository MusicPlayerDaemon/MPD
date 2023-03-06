// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Config.hxx"
#include "config/Data.hxx"

#define CLIENT_TIMEOUT_DEFAULT			(60)
#define CLIENT_MAX_COMMAND_LIST_DEFAULT		(2048*1024)
#define CLIENT_MAX_OUTPUT_BUFFER_SIZE_DEFAULT	(8192*1024)

Event::Duration client_timeout;
size_t client_max_command_list_size;
size_t client_max_output_buffer_size;

void
client_manager_init(const ConfigData &config)
{
	unsigned client_timeout_s = config.GetPositive(ConfigOption::CONN_TIMEOUT,
						       CLIENT_TIMEOUT_DEFAULT);
	client_timeout = std::chrono::seconds(client_timeout_s);

	client_max_command_list_size =
		config.GetPositive(ConfigOption::MAX_COMMAND_LIST_SIZE,
				   CLIENT_MAX_COMMAND_LIST_DEFAULT / 1024)
		* 1024;

	client_max_output_buffer_size =
		config.GetPositive(ConfigOption::MAX_OUTPUT_BUFFER_SIZE,
				   CLIENT_MAX_OUTPUT_BUFFER_SIZE_DEFAULT / 1024)
		* 1024;
}
