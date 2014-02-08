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
#include "NfsInputPlugin.hxx"
#include "../InputStream.hxx"
#include "../InputPlugin.hxx"
#include "lib/nfs/Domain.hxx"
#include "util/StringUtil.hxx"
#include "util/Error.hxx"

extern "C" {
#include <nfsc/libnfs.h>
}

#include <sys/stat.h>
#include <fcntl.h>

class NfsInputStream {
	InputStream base;

	nfs_context *ctx;
	nfsfh *fh;

public:
	NfsInputStream(const char *uri,
		       Mutex &mutex, Cond &cond,
		       nfs_context *_ctx, nfsfh *_fh,
		       InputStream::offset_type size)
		:base(input_plugin_nfs, uri, mutex, cond),
		 ctx(_ctx), fh(_fh) {
		base.ready = true;
		base.seekable = true;
		base.size = size;
	}

	~NfsInputStream() {
		nfs_close(ctx, fh);
		nfs_destroy_context(ctx);
	}

	InputStream *GetBase() {
		return &base;
	}

	bool IsEOF() const {
		return base.offset >= base.size;
	}

	size_t Read(void *ptr, size_t size, Error &error) {
		int nbytes = nfs_read(ctx, fh, size, (char *)ptr);
		if (nbytes < 0) {
			error.SetErrno(-nbytes, "nfs_read() failed");
			nbytes = 0;
		}

		return nbytes;
	}

	bool Seek(InputStream::offset_type offset, int whence, Error &error) {
		uint64_t current_offset;
		int result = nfs_lseek(ctx, fh, offset, whence, &current_offset);
		if (result < 0) {
			error.SetErrno(-result, "smbc_lseek() failed");
			return false;
		}

		base.offset = current_offset;
		return true;
	}
};

/*
 * InputPlugin methods
 *
 */

static InputStream *
input_nfs_open(const char *uri,
	       Mutex &mutex, Cond &cond,
	       Error &error)
{
	if (!StringStartsWith(uri, "nfs://"))
		return nullptr;

	uri += 6;

	const char *slash = strchr(uri, '/');
	if (slash == nullptr) {
		error.Set(nfs_domain, "Malformed nfs:// URI");
		return nullptr;
	}

	const std::string server(uri, slash);

	uri = slash;
	slash = strrchr(uri + 1, '/');
	if (slash == nullptr || slash[1] == 0) {
		error.Set(nfs_domain, "Malformed nfs:// URI");
		return nullptr;
	}

	const std::string mount(uri, slash);
	uri = slash;

	nfs_context *ctx = nfs_init_context();
	if (ctx == nullptr) {
		error.Set(nfs_domain, "nfs_init_context() failed");
		return nullptr;
	}

	int result = nfs_mount(ctx, server.c_str(), mount.c_str());
	if (result < 0) {
		nfs_destroy_context(ctx);
		error.SetErrno(-result, "nfs_mount() failed");
		return nullptr;
	}

	nfsfh *fh;
	result = nfs_open(ctx, uri, O_RDONLY, &fh);
	if (result < 0) {
		nfs_destroy_context(ctx);
		error.SetErrno(-result, "nfs_open() failed");
		return nullptr;
	}

	struct stat st;
	result = nfs_fstat(ctx, fh, &st);
	if (result < 0) {
		nfs_close(ctx, fh);
		nfs_destroy_context(ctx);
		error.SetErrno(-result, "nfs_fstat() failed");
		return nullptr;
	}

	auto is = new NfsInputStream(uri, mutex, cond, ctx, fh, st.st_size);
	return is->GetBase();
}

static size_t
input_nfs_read(InputStream *is, void *ptr, size_t size,
		     Error &error)
{
	NfsInputStream &s = *(NfsInputStream *)is;
	return s.Read(ptr, size, error);
}

static void
input_nfs_close(InputStream *is)
{
	NfsInputStream *s = (NfsInputStream *)is;
	delete s;
}

static bool
input_nfs_eof(InputStream *is)
{
	NfsInputStream &s = *(NfsInputStream *)is;
	return s.IsEOF();
}

static bool
input_nfs_seek(InputStream *is,
		     InputPlugin::offset_type offset, int whence,
		     Error &error)
{
	NfsInputStream &s = *(NfsInputStream *)is;
	return s.Seek(offset, whence, error);
}

const InputPlugin input_plugin_nfs = {
	"nfs",
	nullptr,
	nullptr,
	input_nfs_open,
	input_nfs_close,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	input_nfs_read,
	input_nfs_eof,
	input_nfs_seek,
};
