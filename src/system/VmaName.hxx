// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include <cstddef> // for std::byte
#include <span>

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
SetVmaName(std::span<const std::byte> vma, const char *name)
{
#ifdef __linux__
	prctl(PR_SET_VMA, PR_SET_VMA_ANON_NAME,
	      (unsigned long)vma.data(), vma.size(),
	      (unsigned long)name);
#else
	(void)vma;
	(void)name;
#endif
}
