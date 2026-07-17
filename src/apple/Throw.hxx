// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include <CoreFoundation/CFBase.h>

namespace Apple {

void
ThrowOSStatus(OSStatus status);

void
ThrowOSStatus(OSStatus status, const char *msg);

} // namespace Apple
