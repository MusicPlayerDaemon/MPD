// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

namespace Curl { struct StringResponse; }

[[noreturn]]
void
ThrowQobuzError(const Curl::StringResponse &response);
