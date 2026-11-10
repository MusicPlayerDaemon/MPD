// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include "thread/Mutex.hxx"

#include <chrono>

class InputStream;

void
WaitReady(InputStream &is, std::unique_lock<Mutex> &lock);

void
LockWaitReady(InputStream &is);

/**
 * Like WaitReady(), but gives up after the specified duration and
 * throws an exception.
 */
void
WaitReady(InputStream &is, std::unique_lock<Mutex> &lock,
	  std::chrono::steady_clock::duration timeout);

void
LockWaitReady(InputStream &is, std::chrono::steady_clock::duration timeout);
