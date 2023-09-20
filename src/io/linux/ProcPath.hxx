// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "lib/fmt/ToBuffer.hxx"
#include "io/FileDescriptor.hxx"
#include "util/StringBuffer.hxx"

/**
 * Build the path to the "/proc/self/fd/" magic link of the given file
 * descriptor.
 */
[[gnu::const]]
inline StringBuffer<32>
ProcFdPath(FileDescriptor fd) noexcept
{
	return FmtBuffer<32>("/proc/self/fd/{}", fd.Get());
}

/**
 * Build the path to the "/proc/self/fdinfo/" file of the given file
 * descriptor.
 */
[[gnu::const]]
inline StringBuffer<32>
ProcFdinfoPath(FileDescriptor fd) noexcept
{
	return FmtBuffer<32>("/proc/self/fdinfo/{}", fd.Get());
}
