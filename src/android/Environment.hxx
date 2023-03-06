// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include <jni.h>

class AllocatedPath;

namespace Environment {

void
Initialise(JNIEnv *env) noexcept;

void
Deinitialise(JNIEnv *env) noexcept;

/**
 * Determine the mount point of the external SD card.
 */
[[gnu::pure]]
AllocatedPath
getExternalStorageDirectory(JNIEnv *env) noexcept;

[[gnu::pure]]
AllocatedPath
getExternalStoragePublicDirectory(JNIEnv *env, const char *type) noexcept;

} // namespace Environment
