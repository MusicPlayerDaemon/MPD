// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#ifdef __linux__
# include <sys/prctl.h>

/* fallback definitions if our libc is older than the kernel */
# ifndef PR_SET_VMA
#  define PR_SET_VMA 0x53564d41
# endif
# ifndef PR_SET_VMA_ANON_NAME
#  define PR_SET_VMA_ANON_NAME 0
# endif
#endif // __linux__

/**
 * Set a name for the specified virtual memory area.
 *
 * This feature requires Linux 5.17.
 */
inline void
SetVmaName(const void *start, size_t len, const char *name)
{
#ifdef __linux__
	prctl(PR_SET_VMA, PR_SET_VMA_ANON_NAME, (unsigned long)start, len,
	      (unsigned long)name);
#else
	(void)start;
	(void)len;
	(void)name;
#endif
}
