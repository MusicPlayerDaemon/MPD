// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

class KernelVersionCode {
	unsigned value = 0;

public:
	constexpr KernelVersionCode() noexcept = default;

	constexpr KernelVersionCode(unsigned major,
				    unsigned minor=0,
				    unsigned patch=0) noexcept
		:value((major << 16) | (minor << 8) | patch) {}

	constexpr bool operator>=(KernelVersionCode other) const noexcept {
		return value >= other.value;
	}
};

/**
 * Is the currently running Linux kernel at least the given version?
 */
[[gnu::const]]
bool
IsKernelVersionOrNewer(KernelVersionCode v) noexcept;
