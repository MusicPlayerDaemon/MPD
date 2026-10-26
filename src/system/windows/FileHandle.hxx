// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include "util/TagStructs.hxx"

#include <handleapi.h>

#include <cassert>
#include <utility> // for std::exchange()

/**
 * OO wrapper for a Windows #HANDLE (for those handles that use
 * #INVALID_HANDLE_VALUE, not #nullptr).
 */
class FileHandle {
	HANDLE handle;

public:
	constexpr FileHandle() noexcept = default;

	explicit constexpr FileHandle(HANDLE _handle) noexcept
		:handle(_handle) {}

	static constexpr FileHandle Undefined() noexcept {
		return FileHandle{INVALID_HANDLE_VALUE};
	}

	constexpr bool IsDefined() const noexcept {
		return handle != INVALID_HANDLE_VALUE;
	}

	constexpr HANDLE Get() const noexcept {
		return handle;
	}

	[[nodiscard]]
	HANDLE Steal() noexcept {
		return std::exchange(handle, INVALID_HANDLE_VALUE);
	}

	void Close() noexcept {
		assert(IsDefined());

		::CloseHandle(Steal());
	}
};

/**
 * RAII for a Windows #HANDLE (for those handles that use
 * #INVALID_HANDLE_VALUE).
 *
 * @see https://docs.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-createeventw
 */
class UniqueFileHandle : public FileHandle {
public:
	[[nodiscard]]
	UniqueFileHandle() noexcept
		:FileHandle(INVALID_HANDLE_VALUE) {}

	[[nodiscard]]
	explicit UniqueFileHandle(AdoptTag, HANDLE _handle) noexcept
		:FileHandle(_handle) {}

	[[nodiscard]]
	explicit UniqueFileHandle(AdoptTag, FileHandle _handle) noexcept
		:FileHandle(_handle) {}

	[[nodiscard]]
	UniqueFileHandle(UniqueFileHandle &&src) noexcept
		:FileHandle(src.Steal()) {}

	~UniqueFileHandle() noexcept {
		if (IsDefined())
			Close();
	}

	[[nodiscard]]
	FileHandle Release() noexcept {
		return std::exchange(*static_cast<FileHandle *>(this), FileHandle::Undefined());
	}

	UniqueFileHandle &operator=(UniqueFileHandle &&src) noexcept {
		if (IsDefined())
			Close();

		FileHandle::operator=(src.Release());
		return *this;
	}
};
