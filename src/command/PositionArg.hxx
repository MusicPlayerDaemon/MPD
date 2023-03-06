// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

struct playlist;
struct RangeArg;

/**
 * Throws #ProtocolError on error.
 */
unsigned
ParseInsertPosition(const char *s, const playlist &playlist);

unsigned
ParseMoveDestination(const char *s, const RangeArg range, const playlist &p);
