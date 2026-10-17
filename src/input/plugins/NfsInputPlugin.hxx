// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include "input/Ptr.hxx"
#include "thread/Mutex.hxx"

#include <string_view>

extern const struct InputPlugin input_plugin_nfs;

InputStreamPtr
OpenNfsInputStream(std::string_view uri, Mutex &mutex);
