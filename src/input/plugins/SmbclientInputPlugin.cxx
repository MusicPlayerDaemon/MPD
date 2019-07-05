/*
 * Copyright 2003-2019 The Music Player Daemon Project
 * http://www.musicpd.org
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "SmbclientInputPlugin.hxx"
#include "lib/smbclient/Init.hxx"
#include "lib/smbclient/Mutex.hxx"
#include "../InputStream.hxx"
#include "../InputPlugin.hxx"
#include "../MaybeBufferedInputStream.hxx"
#include "PluginUnavailable.hxx"
#include "system/Error.hxx"

#include <libsmbclient.h>

class SmbclientInputStream final : public InputStream {
	SMBCCTX *ctx;
	int fd;

public:
	SmbclientInputStream(const char *_uri,
			     Mutex &_mutex,
			     SMBCCTX *_ctx, int _fd, const struct stat &st)
		:InputStream(_uri, _mutex),
		 ctx(_ctx), fd(_fd) {
		seekable = true;
		size = st.st_size;
		SetReady();
	}

	~SmbclientInputStream() {
		const std::lock_guard<Mutex> lock(smbclient_mutex);
		smbc_close(fd);
		smbc_free_context(ctx, 1);
	}

	/* virtual methods from InputStream */

	bool IsEOF() const noexcept override {
		return offset >= size;
	}

	size_t Read(std::unique_lock<Mutex> &lock,
		    void *ptr, size_t size) override;
	void Seek(std::unique_lock<Mutex> &lock, offset_type offset) override;
};

/*
 * InputPlugin methods
 *
 */

static void
input_smbclient_init(EventLoop &, const ConfigBlock &)
{
	try {
		SmbclientInit();
	} catch (...) {
		std::throw_with_nested(PluginUnavailable("libsmbclient initialization failed"));
	}

	// TODO: create one global SMBCCTX here?

	// TODO: evaluate ConfigBlock, call smbc_setOption*()
}

static InputStreamPtr
input_smbclient_open(const char *uri,
		     Mutex &mutex)
{
	const std::lock_guard<Mutex> protect(smbclient_mutex);

	SMBCCTX *ctx = smbc_new_context();
	if (ctx == nullptr)
		throw MakeErrno("smbc_new_context() failed");

	SMBCCTX *ctx2 = smbc_init_context(ctx);
	if (ctx2 == nullptr) {
		int e = errno;
		smbc_free_context(ctx, 1);
		throw MakeErrno(e, "smbc_init_context() failed");
	}

	ctx = ctx2;

	int fd = smbc_open(uri, O_RDONLY, 0);
	if (fd < 0) {
		int e = errno;
		smbc_free_context(ctx, 1);
		throw MakeErrno(e, "smbc_open() failed");
	}

	struct stat st;
	if (smbc_fstat(fd, &st) < 0) {
		int e = errno;
		smbc_free_context(ctx, 1);
		throw MakeErrno(e, "smbc_fstat() failed");
	}

	return std::make_unique<MaybeBufferedInputStream>
		(std::make_unique<SmbclientInputStream>(uri, mutex,
							ctx, fd, st));
}

size_t
SmbclientInputStream::Read(std::unique_lock<Mutex> &,
			   void *ptr, size_t read_size)
{
	ssize_t nbytes;

	{
		const ScopeUnlock unlock(mutex);
		const std::lock_guard<Mutex> lock(smbclient_mutex);
		nbytes = smbc_read(fd, ptr, read_size);
	}

	if (nbytes < 0)
		throw MakeErrno("smbc_read() failed");

	offset += nbytes;
	return nbytes;
}

void
SmbclientInputStream::Seek(std::unique_lock<Mutex> &,
			   offset_type new_offset)
{
	off_t result;

	{
		const ScopeUnlock unlock(mutex);
		const std::lock_guard<Mutex> lock(smbclient_mutex);
		result = smbc_lseek(fd, new_offset, SEEK_SET);
	}

	if (result < 0)
		throw MakeErrno("smbc_lseek() failed");

	offset = result;
}

static constexpr const char *smbclient_prefixes[] = {
	"smb://",
	nullptr
};

const InputPlugin input_plugin_smbclient = {
	"smbclient",
	smbclient_prefixes,
	input_smbclient_init,
	nullptr,
	input_smbclient_open,
	nullptr
};
