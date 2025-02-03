// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <fcntl.h> // for O_*
#include <linux/openat2.h> // for RESOLVE_*
#include <sys/syscall.h>
#include <unistd.h>

static inline int
openat2(int dirfd, const char *pathname,
	const struct open_how *how, size_t size)
{
	return syscall(__NR_openat2, dirfd, pathname, how, size);
}
