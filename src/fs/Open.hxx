// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

class Path;
class UniqueFileHandle;

/**
 * Open the specified path as read-only #UniqueFileHandle;
 *
 * Throws on error.
 */
UniqueFileHandle
OpenReadOnly(Path path);
