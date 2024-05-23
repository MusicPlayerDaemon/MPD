// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_SMBCLIENT_CONTEXT_HXX
#define MPD_SMBCLIENT_CONTEXT_HXX

#include "thread/Mutex.hxx"

#include <libsmbclient.h>

#include <utility>

/**
 * Wrapper for `SMBCCTX*`.
 */
class SmbclientContext {
	/**
	 * This mutex protects the libsmbclient functions
	 * smbc_new_context() and smbc_free_context() which need to be
	 * serialized.  We need to do this because we can't use
	 * smbc_thread_posix(), which is not exported by libsmbclient.
	 */
	static Mutex global_mutex;

	SMBCCTX *ctx = nullptr;

	explicit SmbclientContext(SMBCCTX *_ctx) noexcept
		:ctx(_ctx) {}

public:
	SmbclientContext() = default;

	~SmbclientContext() noexcept {
		if (ctx != nullptr) {
			const std::scoped_lock protect{global_mutex};
			smbc_free_context(ctx, 1);
		}
	}

	SmbclientContext(SmbclientContext &&src) noexcept
		:ctx(std::exchange(src.ctx, nullptr)) {}

	SmbclientContext &operator=(SmbclientContext &&src) noexcept {
		using std::swap;
		swap(ctx, src.ctx);
		return *this;
	}

	/**
	 * Throws on error.
	 */
	static SmbclientContext New();

	SMBCFILE *Open(const char *fname, int flags, mode_t mode) noexcept {
		return smbc_getFunctionOpen(ctx)(ctx, fname, flags, mode);
	}

	SMBCFILE *OpenReadOnly(const char *fname) noexcept {
		return Open(fname, O_RDONLY, 0);
	}

	ssize_t Read(SMBCFILE *file, void *buf, size_t count) noexcept {
		return smbc_getFunctionRead(ctx)(ctx, file, buf, count);
	}

	off_t Seek(SMBCFILE *file, off_t offset, int whence=SEEK_SET) noexcept {
		return smbc_getFunctionLseek(ctx)(ctx, file, offset, whence);
	}

	int Stat(const char *fname, struct stat &st) noexcept {
		return smbc_getFunctionStat(ctx)(ctx, fname, &st);
	}

	int Stat(SMBCFILE *file, struct stat &st) noexcept {
		return smbc_getFunctionFstat(ctx)(ctx, file, &st);
	}

	void Close(SMBCFILE *file) noexcept {
		smbc_getFunctionClose(ctx)(ctx, file);
	}

	SMBCFILE *OpenDirectory(const char *fname) noexcept {
		return smbc_getFunctionOpendir(ctx)(ctx, fname);
	}

	void CloseDirectory(SMBCFILE *dir) noexcept {
		smbc_getFunctionClosedir(ctx)(ctx, dir);
	}

	const struct smbc_dirent *ReadDirectory(SMBCFILE *dir) noexcept {
		return smbc_getFunctionReaddir(ctx)(ctx, dir);
	}
};

#endif
