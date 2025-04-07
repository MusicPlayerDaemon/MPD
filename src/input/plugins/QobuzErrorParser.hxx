// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

struct StringCurlResponse;

[[noreturn]]
void
ThrowQobuzError(const StringCurlResponse &response);
