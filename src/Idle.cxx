// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

/*
 * Support library for the "idle" command.
 *
 */

#include "Idle.hxx"
#include "Main.hxx"
#include "Instance.hxx"

#include <cassert>

void
idle_add(unsigned flags)
{
	assert(flags != 0);

	global_instance->EmitIdle(flags);
}
