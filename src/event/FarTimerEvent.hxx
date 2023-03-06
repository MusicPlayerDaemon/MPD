// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "FineTimerEvent.hxx"

/**
 * A coarse timer event which schedules far into the future.  Use this
 * when you need a coarse resolution, but the supported time span of
 * #CoarseTimerEvent is not enough.  For example, a good use case is
 * timers which fire only every few minutes and do periodic cleanup.
 *
 * Right now, this is just an alias for #FineTimerEvent.  This class
 * supports arbitrary time spans, but uses a high-resolution timer.
 * Eventually, we may turn this into a timer wheel with minute
 * resolution.
 */
using FarTimerEvent = FineTimerEvent;
