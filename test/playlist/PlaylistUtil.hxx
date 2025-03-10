// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include <memory>
#include <string>

class SongEnumerator;

std::unique_ptr<SongEnumerator>
ParsePlaylist(const char *uri, std::string_view contents);

std::string
ToString(SongEnumerator &e);
