// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_LISTEN_HXX
#define MPD_LISTEN_HXX

struct ConfigData;
class ClientListener;

extern int listen_port;

void
listen_global_init(const ConfigData &config, ClientListener &listener);

#endif
