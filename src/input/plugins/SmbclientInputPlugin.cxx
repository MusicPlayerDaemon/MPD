/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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
#include "util/StringUtil.hxx"
#include "util/Error.hxx"

#include <libsmbclient.h>

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

	size_t Read(void *ptr, size_t size, Error &error) override;
	bool Seek(offset_type offset, Error &error) override;
};

/*
 * InputPlugin methods
 *
 */

static InputPlugin::InitResult
input_smbclient_init(gcc_unused const config_param &param, Error &error)
{
	if (!SmbclientInit(error))
		return InputPlugin::InitResult::UNAVAILABLE;

	// TODO: create one global SMBCCTX here?

	// TODO: evaluate config_param, call smbc_setOption*()

	return InputPlugin::InitResult::SUCCESS;
}

static InputStream *
input_smbclient_open(const char *uri,
		     Mutex &mutex, Cond &cond,
		     Error &error)
{
	if (!StringStartsWith(uri, "smb://"))
		return nullptr;

	const ScopeLock protect(smbclient_mutex);

	SMBCCTX *ctx = smbc_new_context();
	if (ctx == nullptr) {
		error.SetErrno("smbc_new_context() failed");
		return nullptr;
	}

	SMBCCTX *ctx2 = smbc_init_context(ctx);
	if (ctx2 == nullptr) {
		error.SetErrno("smbc_init_context() failed");
		smbc_free_context(ctx, 1);
		return nullptr;
	}

	ctx = ctx2;

	int fd = smbc_open(uri, O_RDONLY, 0);
	if (fd < 0) {
		error.SetErrno("smbc_open() failed");
		smbc_free_context(ctx, 1);
		return nullptr;
	}

	struct stat st;
	if (smbc_fstat(fd, &st) < 0) {
		error.SetErrno("smbc_fstat() failed");
		smbc_close(fd);
		smbc_free_context(ctx, 1);
		return nullptr;
	}

	return new SmbclientInputStream(uri, mutex, cond, ctx, fd, st);
}

size_t
SmbclientInputStream::Read(void *ptr, size_t read_size, Error &error)
{
	smbclient_mutex.lock();
	ssize_t nbytes = smbc_read(fd, ptr, read_size);
	smbclient_mutex.unlock();
	if (nbytes < 0) {
		error.SetErrno("smbc_read() failed");
		nbytes = 0;
	}

	offset += nbytes;
	return nbytes;
}

bool
SmbclientInputStream::Seek(offset_type new_offset, Error &error)
{
	smbclient_mutex.lock();
	off_t result = smbc_lseek(fd, new_offset, SEEK_SET);
	smbclient_mutex.unlock();
	if (result < 0) {
		error.SetErrno("smbc_lseek() failed");
		return false;
	}

	offset = result;
	return true;
}

const InputPlugin input_plugin_smbclient = {
	"smbclient",
	input_smbclient_init,
	nullptr,
	input_smbclient_open,
};
