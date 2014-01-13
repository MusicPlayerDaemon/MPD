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
#include "InputStream.hxx"
#include "InputPlugin.hxx"
#include "util/StringUtil.hxx"
#include "util/Error.hxx"

#include <libsmbclient.h>

#include <string.h>

class SmbclientInputStream {
	InputStream base;

	SMBCCTX *ctx;
	int fd;

public:
	SmbclientInputStream(const char *uri,
			     Mutex &mutex, Cond &cond,
			     SMBCCTX *_ctx, int _fd, const struct stat &st)
		:base(input_plugin_smbclient, uri, mutex, cond),
		 ctx(_ctx), fd(_fd) {
		base.ready = true;
		base.seekable = true;
		base.size = st.st_size;
	}

	~SmbclientInputStream() {
		smbc_close(fd);
		smbc_free_context(ctx, 1);
	}

	InputStream *GetBase() {
		return &base;
	}

	bool IsEOF() const {
		return base.offset >= base.size;
	}

	size_t Read(void *ptr, size_t size, Error &error) {
		ssize_t nbytes = smbc_read(fd, ptr, size);
		if (nbytes < 0) {
			error.SetErrno("smbc_read() failed");
			nbytes = 0;
		}

		return nbytes;
	}

	bool Seek(InputStream::offset_type offset, int whence, Error &error) {
		off_t result = smbc_lseek(fd, offset, whence);
		if (result < 0) {
			error.SetErrno("smbc_lseek() failed");
			return false;
		}

		base.offset = result;
		return true;
	}
};

static void
mpd_smbc_get_auth_data(gcc_unused const char *srv,
		       gcc_unused const char *shr,
		       char *wg, gcc_unused int wglen,
		       char *un, gcc_unused int unlen,
		       char *pw, gcc_unused int pwlen)
{
	// TODO: implement
	strcpy(wg, "WORKGROUP");
	strcpy(un, "foo");
	strcpy(pw, "bar");
}

/*
 * InputPlugin methods
 *
 */

static bool
input_smbclient_init(gcc_unused const config_param &param, Error &error)
{
	constexpr int debug = 0;
	if (smbc_init(mpd_smbc_get_auth_data, debug) < 0) {
		error.SetErrno("smbc_init() failed");
		return false;
	}

	// TODO: create one global SMBCCTX here?

	// TODO: evaluate config_param, call smbc_setOption*()

	return true;
}

static InputStream *
input_smbclient_open(const char *uri,
		     Mutex &mutex, Cond &cond,
		     Error &error)
{
	if (!StringStartsWith(uri, "smb://"))
		return nullptr;

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

	auto s = new SmbclientInputStream(uri, mutex, cond, ctx, fd, st);
	return s->GetBase();
}

static size_t
input_smbclient_read(InputStream *is, void *ptr, size_t size,
		     Error &error)
{
	SmbclientInputStream &s = *(SmbclientInputStream *)is;
	return s.Read(ptr, size, error);
}

static void
input_smbclient_close(InputStream *is)
{
	SmbclientInputStream *s = (SmbclientInputStream *)is;
	delete s;
}

static bool
input_smbclient_eof(InputStream *is)
{
	SmbclientInputStream &s = *(SmbclientInputStream *)is;
	return s.IsEOF();
}

static bool
input_smbclient_seek(InputStream *is,
		     InputPlugin::offset_type offset, int whence,
		     Error &error)
{
	SmbclientInputStream &s = *(SmbclientInputStream *)is;
	return s.Seek(offset, whence, error);
}

const InputPlugin input_plugin_smbclient = {
	"smbclient",
	input_smbclient_init,
	nullptr,
	input_smbclient_open,
	input_smbclient_close,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	input_smbclient_read,
	input_smbclient_eof,
	input_smbclient_seek,
};
