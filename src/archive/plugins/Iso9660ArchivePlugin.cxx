/*
 * Copyright 2003-2016 The Music Player Daemon Project
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

/**
  * iso archive handling (requires cdio, and iso9660)
  */

#include "config.h"
#include "Iso9660ArchivePlugin.hxx"
#include "../ArchivePlugin.hxx"
#include "../ArchiveFile.hxx"
#include "../ArchiveVisitor.hxx"
#include "input/InputStream.hxx"
#include "fs/Path.hxx"
#include "util/RefCount.hxx"
#include "util/RuntimeError.hxx"
#include "util/Error.hxx"
#include "util/Domain.hxx"

#include <cdio/iso9660.h>

#include <stdlib.h>
#include <string.h>

#define CEILING(x, y) ((x+(y-1))/y)

class Iso9660ArchiveFile final : public ArchiveFile {
	RefCount ref;

	iso9660_t *iso;

public:
	Iso9660ArchiveFile(iso9660_t *_iso)
		:ArchiveFile(iso9660_archive_plugin), iso(_iso) {}

	~Iso9660ArchiveFile() {
		iso9660_close(iso);
	}

	void Ref() {
		ref.Increment();
	}

	void Unref() {
		if (ref.Decrement())
			delete this;
	}

	long SeekRead(void *ptr, lsn_t start, long int i_size) const {
		return iso9660_iso_seek_read(iso, ptr, start, i_size);
	}

	/**
	 * @param capacity the path buffer size
	 */
	void Visit(char *path, size_t length, size_t capacity,
		   ArchiveVisitor &visitor);

	virtual void Close() override {
		Unref();
	}

	virtual void Visit(ArchiveVisitor &visitor) override;

	InputStream *OpenStream(const char *path,
				Mutex &mutex, Cond &cond) override;
};

static constexpr Domain iso9660_domain("iso9660");

/* archive open && listing routine */

inline void
Iso9660ArchiveFile::Visit(char *path, size_t length, size_t capacity,
			  ArchiveVisitor &visitor)
{
	auto *entlist = iso9660_ifs_readdir(iso, path);
	if (!entlist) {
		return;
	}
	/* Iterate over the list of nodes that iso9660_ifs_readdir gives  */
	CdioListNode_t *entnode;
	_CDIO_LIST_FOREACH (entnode, entlist) {
		auto *statbuf = (iso9660_stat_t *)
			_cdio_list_node_data(entnode);
		const char *filename = statbuf->filename;
		if (strcmp(filename, ".") == 0 || strcmp(filename, "..") == 0)
			continue;

		size_t filename_length = strlen(filename);
		if (length + filename_length + 1 >= capacity)
			/* file name is too long */
			continue;

		memcpy(path + length, filename, filename_length + 1);
		size_t new_length = length + filename_length;

		if (iso9660_stat_s::_STAT_DIR == statbuf->type ) {
			memcpy(path + new_length, "/", 2);
			Visit(path, new_length + 1, capacity, visitor);
		} else {
			//remove leading /
			visitor.VisitArchiveEntry(path + 1);
		}
	}
	_cdio_list_free (entlist, true);
}

static ArchiveFile *
iso9660_archive_open(Path pathname)
{
	/* open archive */
	auto iso = iso9660_open(pathname.c_str());
	if (iso == nullptr)
		throw FormatRuntimeError("Failed to open ISO9660 file %s",
					 pathname.c_str());

	return new Iso9660ArchiveFile(iso);
}

void
Iso9660ArchiveFile::Visit(ArchiveVisitor &visitor)
{
	char path[4096] = "/";
	Visit(path, 1, sizeof(path), visitor);
}

/* single archive handling */

class Iso9660InputStream final : public InputStream {
	Iso9660ArchiveFile &archive;

	iso9660_stat_t *statbuf;

public:
	Iso9660InputStream(Iso9660ArchiveFile &_archive, const char *_uri,
			   Mutex &_mutex, Cond &_cond,
			   iso9660_stat_t *_statbuf)
		:InputStream(_uri, _mutex, _cond),
		 archive(_archive), statbuf(_statbuf) {
		size = statbuf->size;
		SetReady();

		archive.Ref();
	}

	~Iso9660InputStream() {
		free(statbuf);
		archive.Unref();
	}

	/* virtual methods from InputStream */
	bool IsEOF() override;
	size_t Read(void *ptr, size_t size, Error &error) override;
};

InputStream *
Iso9660ArchiveFile::OpenStream(const char *pathname,
			       Mutex &mutex, Cond &cond)
{
	auto statbuf = iso9660_ifs_stat_translate(iso, pathname);
	if (statbuf == nullptr)
		throw FormatRuntimeError("not found in the ISO file: %s",
					 pathname);

	return new Iso9660InputStream(*this, pathname, mutex, cond,
				      statbuf);
}

size_t
Iso9660InputStream::Read(void *ptr, size_t read_size, Error &error)
{
	int readed = 0;
	int no_blocks, cur_block;
	size_t left_bytes = statbuf->size - offset;

	if (left_bytes < read_size) {
		no_blocks = CEILING(left_bytes, ISO_BLOCKSIZE);
	} else {
		no_blocks = read_size / ISO_BLOCKSIZE;
	}

	if (no_blocks == 0)
		return 0;

	cur_block = offset / ISO_BLOCKSIZE;

	readed = archive.SeekRead(ptr, statbuf->lsn + cur_block,
				  no_blocks);

	if (readed != no_blocks * ISO_BLOCKSIZE) {
		error.Format(iso9660_domain,
			     "error reading ISO file at lsn %lu",
			     (unsigned long)cur_block);
		return 0;
	}
	if (left_bytes < read_size) {
		readed = left_bytes;
	}

	offset += readed;
	return readed;
}

bool
Iso9660InputStream::IsEOF()
{
	return offset == size;
}

/* exported structures */

static const char *const iso9660_archive_extensions[] = {
	"iso",
	nullptr
};

const ArchivePlugin iso9660_archive_plugin = {
	"iso",
	nullptr,
	nullptr,
	iso9660_archive_open,
	iso9660_archive_extensions,
};
