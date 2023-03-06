// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

/*
 * Support library for the "idle" command.
 *
 */

#ifndef MPD_IDLE_HXX
#define MPD_IDLE_HXX

#include "IdleFlags.hxx"

/**
 * Adds idle flag (with bitwise "or") and queues notifications to all
 * clients.
 */
void
idle_add(unsigned flags);

#endif
