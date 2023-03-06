// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "KernelVersion.hxx"

#include <sys/utsname.h>
#include <stdio.h>

[[gnu::const]]
static KernelVersionCode
GetKernelVersionCode() noexcept
{
	struct utsname u;
	if (uname(&u) != 0)
		return {};

	unsigned major, minor, patch;
	switch (sscanf(u.release, "%u.%u.%u", &major, &minor, &patch)) {
	case 1:
		minor = patch = 0;
		break;

	case 2:
		patch = 0;
		break;

	case 3:
		break;

	default:
		return {};
	}

	return {major, minor, patch};
}

bool
IsKernelVersionOrNewer(KernelVersionCode v) noexcept
{
	static const auto kernel_version_code = GetKernelVersionCode();
	return kernel_version_code >= v;
}
