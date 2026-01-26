// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "system/linux/Features.h" // for HAVE_NATIVE_OPENAT2

#include <fcntl.h>

#ifndef HAVE_NATIVE_OPENAT2
/* if the C library does not provide an openat2() wrappper, we roll
   our own */

#include <linux/openat2.h> // for RESOLVE_*
#include <sys/syscall.h>
#include <unistd.h>

static inline int
openat2(int dirfd, const char *pathname,
	const struct open_how *how, size_t size)
{
	return syscall(__NR_openat2, dirfd, pathname, how, size);
}

#endif // !HAVE_NATIVE_OPENAT2
