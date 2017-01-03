/*
 * Copyright 2003-2017 The Music Player Daemon Project
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

#include "config.h"
#include "SmbclientInputPlugin.hxx"
#include "lib/smbclient/Init.hxx"
#include "lib/smbclient/Mutex.hxx"
#include "../InputStream.hxx"
#include "../InputPlugin.hxx"
#include "PluginUnavailable.hxx"
#include "system/Error.hxx"
#include "util/StringCompare.hxx"

#include <libsmbclient.h>

#include <stdexcept>

class SmbclientInputStream final : public InputStream {
	SMBCCTX *ctx;
	int fd;

public:
	SmbclientInputStream(const char *_uri,
			     Mutex &_mutex, Cond &_cond,
			     SMBCCTX *_ctx, int _fd, const struct stat &st)
		:InputStream(_uri, _mutex, _cond),
		 ctx(_ctx), fd(_fd) {
		seekable = true;
		size = st.st_size;
		SetReady();
	}

	~SmbclientInputStream() {
		smbclient_mutex.lock();
		smbc_close(fd);
		smbc_free_context(ctx, 1);
		smbclient_mutex.unlock();
	}

	/* virtual methods from InputStream */

	bool IsEOF() override {
		return offset >= size;
	}

	size_t Read(void *ptr, size_t size) override;
	void Seek(offset_type offset) override;
};

/*
 * InputPlugin methods
 *
 */

static void
input_smbclient_init(gcc_unused const ConfigBlock &block)
{
	try {
		SmbclientInit();
	} catch (const std::runtime_error &e) {
		// TODO: use std::throw_with_nested()?
		throw PluginUnavailable(e.what());
	}

	// TODO: create one global SMBCCTX here?

	// TODO: evaluate ConfigBlock, call smbc_setOption*()
}

static InputStream *
input_smbclient_open(const char *uri,
		     Mutex &mutex, Cond &cond)
{
	if (!StringStartsWith(uri, "smb://"))
		return nullptr;

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

	return new SmbclientInputStream(uri, mutex, cond, ctx, fd, st);
}

size_t
SmbclientInputStream::Read(void *ptr, size_t read_size)
{
	smbclient_mutex.lock();
	ssize_t nbytes = smbc_read(fd, ptr, read_size);
	smbclient_mutex.unlock();
	if (nbytes < 0)
		throw MakeErrno("smbc_read() failed");

	offset += nbytes;
	return nbytes;
}

void
SmbclientInputStream::Seek(offset_type new_offset)
{
	smbclient_mutex.lock();
	off_t result = smbc_lseek(fd, new_offset, SEEK_SET);
	smbclient_mutex.unlock();
	if (result < 0)
		throw MakeErrno("smbc_lseek() failed");

	offset = result;
}

const InputPlugin input_plugin_smbclient = {
	"smbclient",
	input_smbclient_init,
	nullptr,
	input_smbclient_open,
};
