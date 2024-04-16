// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

/**
  * zip archive handling (requires zziplib)
  */

#include "ZzipArchivePlugin.hxx"
#include "../ArchivePlugin.hxx"
#include "../ArchiveFile.hxx"
#include "../ArchiveVisitor.hxx"
#include "input/InputStream.hxx"
#include "lib/fmt/PathFormatter.hxx"
#include "lib/fmt/RuntimeError.hxx"
#include "fs/NarrowPath.hxx"
#include "fs/Path.hxx"
#include "lib/fmt/SystemError.hxx"
#include "util/UTF8.hxx"

#include <zzip/zzip.h>

#include <utility>

struct ZzipDir {
	ZZIP_DIR *const dir;

	explicit ZzipDir(Path path)
		:dir(zzip_dir_open(NarrowPath(path), nullptr)) {
		if (dir == nullptr)
			throw FmtRuntimeError("Failed to open ZIP file {:?}",
					      path);
	}

	~ZzipDir() noexcept {
		zzip_dir_close(dir);
	}

	ZzipDir(const ZzipDir &) = delete;
	ZzipDir &operator=(const ZzipDir &) = delete;
};

class ZzipArchiveFile final : public ArchiveFile {
	std::shared_ptr<ZzipDir> dir;

public:
	template<typename D>
	explicit ZzipArchiveFile(D &&_dir) noexcept
		:dir(std::forward<D>(_dir)) {}

	void Visit(ArchiveVisitor &visitor) override;

	InputStreamPtr OpenStream(const char *path,
				  Mutex &mutex) override;
};

/* archive open && listing routine */

static std::unique_ptr<ArchiveFile>
zzip_archive_open(Path pathname)
{
	return std::make_unique<ZzipArchiveFile>(std::make_shared<ZzipDir>(pathname));
}

inline void
ZzipArchiveFile::Visit(ArchiveVisitor &visitor)
{
	zzip_rewinddir(dir->dir);

	ZZIP_DIRENT dirent;
	while (zzip_dir_read(dir->dir, &dirent))
		//add only files
		if (dirent.st_size > 0 && ValidateUTF8(dirent.d_name))
			visitor.VisitArchiveEntry(dirent.d_name);
}

/* single archive handling */

class ZzipInputStream final : public InputStream {
	std::shared_ptr<ZzipDir> dir;

	ZZIP_FILE *const file;

public:
	template<typename D>
	ZzipInputStream(D &&_dir, const char *_uri,
			Mutex &_mutex,
			ZZIP_FILE *_file)
		:InputStream(_uri, _mutex),
		 dir(std::forward<D>(_dir)), file(_file) {
		//we are seekable (but its not recommendent to do so)
		seekable = true;

		ZZIP_STAT z_stat;
		zzip_file_stat(file, &z_stat);
		size = z_stat.st_size;

		SetReady();
	}

	~ZzipInputStream() noexcept override {
		zzip_file_close(file);
	}

	ZzipInputStream(const ZzipInputStream &) = delete;
	ZzipInputStream &operator=(const ZzipInputStream &) = delete;

	/* virtual methods from InputStream */
	[[nodiscard]] bool IsEOF() const noexcept override;
	size_t Read(std::unique_lock<Mutex> &lock,
		    void *ptr, size_t size) override;
	void Seek(std::unique_lock<Mutex> &lock, offset_type offset) override;
};

InputStreamPtr
ZzipArchiveFile::OpenStream(const char *pathname,
			    Mutex &mutex)
{
	ZZIP_FILE *_file = zzip_file_open(dir->dir, pathname, 0);
	if (_file == nullptr) {
		const auto error = (zzip_error_t)zzip_error(dir->dir);
		switch (error) {
		case ZZIP_ENOENT:
			throw FmtFileNotFound("Failed to open {:?} in ZIP file",
					      pathname);

		default:
			throw FmtRuntimeError("Failed to open {:?} in ZIP file: {}",
					      pathname,
					      zzip_strerror(error));
		}
	}

	return std::make_unique<ZzipInputStream>(dir, pathname,
						 mutex,
						 _file);
}

size_t
ZzipInputStream::Read(std::unique_lock<Mutex> &, void *ptr, size_t read_size)
{
	const ScopeUnlock unlock(mutex);

	zzip_ssize_t nbytes = zzip_file_read(file, ptr, read_size);
	if (nbytes < 0)
		throw std::runtime_error("zzip_file_read() has failed");

	if (nbytes == 0 && !IsEOF())
		throw FmtRuntimeError("Unexpected end of file {:?} at {} of {}",
				      GetURI(), GetOffset(), GetSize());

	offset = zzip_tell(file);
	return nbytes;
}

bool
ZzipInputStream::IsEOF() const noexcept
{
	return offset_type(zzip_tell(file)) == size;
}

void
ZzipInputStream::Seek(std::unique_lock<Mutex> &, offset_type new_offset)
{
	const ScopeUnlock unlock(mutex);

	zzip_off_t ofs = zzip_seek(file, new_offset, SEEK_SET);
	if (ofs < 0)
		throw std::runtime_error("zzip_seek() has failed");

	offset = ofs;
}

/* exported structures */

static constexpr const char *zzip_archive_extensions[] = {
	"zip",
	nullptr
};

const ArchivePlugin zzip_archive_plugin = {
	"zzip",
	nullptr,
	nullptr,
	zzip_archive_open,
	zzip_archive_extensions,
};
