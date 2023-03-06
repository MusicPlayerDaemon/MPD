// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "MemoryDirectoryReader.hxx"

#include <cassert>

const char *
MemoryStorageDirectoryReader::Read() noexcept
{
	if (first)
		first = false;
	else
		entries.pop_front();

	if (entries.empty())
		return nullptr;

	return entries.front().name.c_str();
}

StorageFileInfo
MemoryStorageDirectoryReader::GetInfo([[maybe_unused]] bool follow)
{
	assert(!first);
	assert(!entries.empty());

	return entries.front().info;
}
