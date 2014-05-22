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

#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>

class NfsInputStream final : public InputStream {
	nfs_context *ctx;
	nfsfh *fh;

public:
	NfsInputStream(const char *_uri,
		       Mutex &_mutex, Cond &_cond,
		       nfs_context *_ctx, nfsfh *_fh,
		       InputStream::offset_type _size)
		:InputStream(_uri, _mutex, _cond),
		 ctx(_ctx), fh(_fh) {
		seekable = true;
		size = _size;
		SetReady();
	}

	~NfsInputStream() {
		nfs_close(ctx, fh);
		nfs_destroy_context(ctx);
	}

	/* virtual methods from InputStream */

	bool IsEOF() override {
		return offset >= size;
	}

	size_t Read(void *ptr, size_t size, Error &error) override;
	bool Seek(offset_type offset, Error &error) override;
};

size_t
NfsInputStream::Read(void *ptr, size_t read_size, Error &error)
{
	int nbytes = nfs_read(ctx, fh, read_size, (char *)ptr);
	if (nbytes < 0) {
		error.SetErrno(-nbytes, "nfs_read() failed");
		nbytes = 0;
	}

	return nbytes;
}

bool
NfsInputStream::Seek(offset_type new_offset, Error &error)
{
	uint64_t current_offset;
	int result = nfs_lseek(ctx, fh, new_offset, SEEK_SET,
			       &current_offset);
	if (result < 0) {
		error.SetErrno(-result, "smbc_lseek() failed");
		return false;
	}

	offset = current_offset;
	return true;
}

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

	return new NfsInputStream(uri, mutex, cond, ctx, fh, st.st_size);
}

const InputPlugin input_plugin_nfs = {
	"nfs",
	nullptr,
	nullptr,
	input_nfs_open,
};
