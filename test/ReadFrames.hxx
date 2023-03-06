// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include <cstddef>

class FileDescriptor;

std::size_t
ReadFrames(FileDescriptor fd, void *buffer, std::size_t size,
	   std::size_t frame_size);
