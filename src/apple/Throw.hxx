// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#ifndef APPLE_THROW_HXX
#define APPLE_THROW_HXX

#include <CoreFoundation/CFBase.h>

namespace Apple {

void
ThrowOSStatus(OSStatus status);

void
ThrowOSStatus(OSStatus status, const char *msg);

} // namespace Apple

#endif
