// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

struct ConfigData;
class ClientListener;

void
listen_global_init(const ConfigData &config, ClientListener &listener);
