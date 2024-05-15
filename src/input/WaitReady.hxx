// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include "thread/Mutex.hxx"

class InputStream;

void
WaitReady(InputStream &is, std::unique_lock<Mutex> &lock);

void
LockWaitReady(InputStream &is);
