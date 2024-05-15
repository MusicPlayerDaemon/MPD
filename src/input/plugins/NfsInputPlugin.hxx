// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include "input/Ptr.hxx"
#include "thread/Mutex.hxx"

#include <string_view>

class NfsConnection;

extern const struct InputPlugin input_plugin_nfs;

InputStreamPtr
OpenNfsInputStream(NfsConnection &connection, std::string_view path,
		   Mutex &mutex);
