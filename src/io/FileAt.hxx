// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "FileDescriptor.hxx"

/**
 * Reference to a file by an anchor directory (which can be an
 * `O_PATH` descriptor) and a path name relative to it.
 */
struct FileAt {
	FileDescriptor directory;
	const char *name;
};
