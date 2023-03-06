// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_CLIENT_CONFIG_HXX
#define MPD_CLIENT_CONFIG_HXX

#include "event/Chrono.hxx"

struct ConfigData;

extern Event::Duration client_timeout;
extern size_t client_max_command_list_size;
extern size_t client_max_output_buffer_size;

void
client_manager_init(const ConfigData &config);

#endif
