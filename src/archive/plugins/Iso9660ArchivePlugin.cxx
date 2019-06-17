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

/**
  * iso archive handling (requires cdio, and iso9660)
  */

#include "Iso9660ArchivePlugin.hxx"
#include "../ArchivePlugin.hxx"
#include "../ArchiveFile.hxx"
#include "../ArchiveVisitor.hxx"
#include "input/InputStream.hxx"
#include "fs/Path.hxx"
#include "util/RuntimeError.hxx"

#include <cdio/iso9660.h>

#include <stdlib.h>
#include <string.h>

#define CEILING(x, y) ((x+(y-1))/y)

struct Iso9660 {
	iso9660_t *const iso;

	explicit Iso9660(Path path)
		:iso(iso9660_open(path.c_str())) {
		if (iso == nullptr)
			throw FormatRuntimeError("Failed to open ISO9660 file %s",
						 path.c_str());
	}

	~Iso9660() noexcept {
		iso9660_close(iso);
	}

	Iso9660(const Iso9660 &) = delete;
	Iso9660 &operator=(const Iso9660 &) = delete;

	long SeekRead(void *ptr, lsn_t start, long int i_size) const {
		return iso9660_iso_seek_read(iso, ptr, start, i_size);
	}
};

class Iso9660ArchiveFile final : public ArchiveFile {
	std::shared_ptr<Iso9660> iso;

public:
	Iso9660ArchiveFile(std::shared_ptr<Iso9660> &&_iso)
		:iso(std::move(_iso)) {}

	/**
	 * @param capacity the path buffer size
	 */
	void Visit(char *path, size_t length, size_t capacity,
		   ArchiveVisitor &visitor);

	virtual void Visit(ArchiveVisitor &visitor) override;

	InputStreamPtr OpenStream(const char *path,
				  Mutex &mutex) override;
};

/* archive open && listing routine */

inline void
Iso9660ArchiveFile::Visit(char *path, size_t length, size_t capacity,
			  ArchiveVisitor &visitor)
{
	auto *entlist = iso9660_ifs_readdir(iso->iso, path);
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

#if LIBCDIO_VERSION_NUM >= 20000
	iso9660_filelist_free(entlist);
#else
	_cdio_list_free (entlist, true);
#endif
}

static std::unique_ptr<ArchiveFile>
iso9660_archive_open(Path pathname)
{
	return std::make_unique<Iso9660ArchiveFile>(std::make_shared<Iso9660>(pathname));
}

void
Iso9660ArchiveFile::Visit(ArchiveVisitor &visitor)
{
	char path[4096] = "/";
	Visit(path, 1, sizeof(path), visitor);
}

/* single archive handling */

class Iso9660InputStream final : public InputStream {
	std::shared_ptr<Iso9660> iso;

	iso9660_stat_t *statbuf;

public:
	Iso9660InputStream(const std::shared_ptr<Iso9660> &_iso,
			   const char *_uri,
			   Mutex &_mutex,
			   iso9660_stat_t *_statbuf)
		:InputStream(_uri, _mutex),
		 iso(_iso), statbuf(_statbuf) {
		size = statbuf->size;
		seekable = true;
		SetReady();
	}

	~Iso9660InputStream() {
		free(statbuf);
	}

	/* virtual methods from InputStream */
	bool IsEOF() const noexcept override;
	size_t Read(std::unique_lock<Mutex> &lock,
		    void *ptr, size_t size) override;

	void Seek(std::unique_lock<Mutex> &, offset_type new_offset) override {
		offset = new_offset;
	}
};

InputStreamPtr
Iso9660ArchiveFile::OpenStream(const char *pathname,
			       Mutex &mutex)
{
	auto statbuf = iso9660_ifs_stat_translate(iso->iso, pathname);
	if (statbuf == nullptr)
		throw FormatRuntimeError("not found in the ISO file: %s",
					 pathname);

	return std::make_unique<Iso9660InputStream>(iso, pathname, mutex,
						    statbuf);
}

size_t
Iso9660InputStream::Read(std::unique_lock<Mutex> &,
			 void *ptr, size_t read_size)
{
	const ScopeUnlock unlock(mutex);

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

	readed = iso->SeekRead(ptr, statbuf->lsn + cur_block, no_blocks);

	if (readed != no_blocks * ISO_BLOCKSIZE)
		throw FormatRuntimeError("error reading ISO file at lsn %lu",
					 (unsigned long)cur_block);

	if (left_bytes < read_size) {
		readed = left_bytes;
	}

	offset += readed;
	return readed;
}

bool
Iso9660InputStream::IsEOF() const noexcept
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
