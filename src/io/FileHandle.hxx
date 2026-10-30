// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#ifdef _WIN32

#include "system/windows/FileHandle.hxx"

#else

#include "FileDescriptor.hxx"
#include "UniqueFileDescriptor.hxx"

class FileHandle : public FileDescriptor {
public:
	using FileDescriptor::FileDescriptor;

	constexpr FileHandle(FileDescriptor src) noexcept
		:FileDescriptor(src) {}
};

class UniqueFileHandle : public UniqueFileDescriptor {
public:
	using UniqueFileDescriptor::UniqueFileDescriptor;

	UniqueFileHandle(UniqueFileDescriptor &&src) noexcept
		:UniqueFileDescriptor(std::move(src)) {}
};

#endif
