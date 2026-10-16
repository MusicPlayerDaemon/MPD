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
 * Throws #TimeoutError if the timeout expires.
 */
void
WaitReady(InputStream &is, std::unique_lock<Mutex> &lock,
	  std::chrono::steady_clock::duration timeout);

/**
 * Throws #TimeoutError if the timeout expires.
 */
void
LockWaitReady(InputStream &is,
	      std::chrono::steady_clock::duration timeout);
