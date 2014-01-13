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

#ifndef MPD_INPUT_STREAM_HXX
#define MPD_INPUT_STREAM_HXX

#include "check.h"
#include "thread/Mutex.hxx"
#include "Compiler.h"

#include <string>

#include <assert.h>
#include <stdint.h>

class Cond;
class Error;
struct Tag;
struct InputPlugin;

struct InputStream {
	typedef int64_t offset_type;

	/**
	 * the plugin which implements this input stream
	 */
	const InputPlugin &plugin;

	/**
	 * The absolute URI which was used to open this stream.
	 */
	std::string uri;

	/**
	 * A mutex that protects the mutable attributes of this object
	 * and its implementation.  It must be locked before calling
	 * any of the public methods.
	 *
	 * This object is allocated by the client, and the client is
	 * responsible for freeing it.
	 */
	Mutex &mutex;

	/**
	 * A cond that gets signalled when the state of this object
	 * changes from the I/O thread.  The client of this object may
	 * wait on it.  Optional, may be nullptr.
	 *
	 * This object is allocated by the client, and the client is
	 * responsible for freeing it.
	 */
	Cond &cond;

	/**
	 * indicates whether the stream is ready for reading and
	 * whether the other attributes in this struct are valid
	 */
	bool ready;

	/**
	 * if true, then the stream is fully seekable
	 */
	bool seekable;

	/**
	 * the size of the resource, or -1 if unknown
	 */
	offset_type size;

	/**
	 * the current offset within the stream
	 */
	offset_type offset;

	/**
	 * the MIME content type of the resource, or empty if unknown.
	 */
	std::string mime;

	InputStream(const InputPlugin &_plugin,
		    const char *_uri, Mutex &_mutex, Cond &_cond)
		:plugin(_plugin), uri(_uri),
		 mutex(_mutex), cond(_cond),
		 ready(false), seekable(false),
		 size(-1), offset(0) {
		assert(_uri != nullptr);
	}

	/**
	 * Opens a new input stream.  You may not access it until the "ready"
	 * flag is set.
	 *
	 * @param mutex a mutex that is used to protect this object; must be
	 * locked before calling any of the public methods
	 * @param cond a cond that gets signalled when the state of
	 * this object changes; may be nullptr if the caller doesn't want to get
	 * notifications
	 * @return an #InputStream object on success, nullptr on error
	 */
	gcc_nonnull_all
	gcc_malloc
	static InputStream *Open(const char *uri, Mutex &mutex, Cond &cond,
				 Error &error);

	/**
	 * Just like Open(), but waits for the stream to become ready.
	 * It is a wrapper for Open(), WaitReady() and Check().
	 */
	gcc_malloc gcc_nonnull_all
	static InputStream *OpenReady(const char *uri,
				      Mutex &mutex, Cond &cond,
				      Error &error);

	/**
	 * Close the input stream and free resources.
	 *
	 * The caller must not lock the mutex.
	 */
	void Close();

	void Lock() {
		mutex.lock();
	}

	void Unlock() {
		mutex.unlock();
	}

	/**
	 * Check for errors that may have occurred in the I/O thread.
	 *
	 * @return false on error
	 */
	bool Check(Error &error);

	/**
	 * Update the public attributes.  Call before accessing attributes
	 * such as "ready" or "offset".
	 */
	void Update();

	/**
	 * Wait until the stream becomes ready.
	 *
	 * The caller must lock the mutex.
	 */
	void WaitReady();

	/**
	 * Wrapper for WaitReady() which locks and unlocks the mutex;
	 * the caller must not be holding it already.
	 */
	void LockWaitReady();

	gcc_pure
	const char *GetMimeType() const {
		assert(ready);

		return mime.empty() ? nullptr : mime.c_str();
	}

	gcc_nonnull_all
	void OverrideMimeType(const char *_mime) {
		assert(ready);

		mime = _mime;
	}

	gcc_pure
	offset_type GetSize() const {
		assert(ready);

		return size;
	}

	gcc_pure
	offset_type GetOffset() const {
		assert(ready);

		return offset;
	}

	gcc_pure
	bool IsSeekable() const {
		assert(ready);

		return seekable;
	}

	/**
	 * Determines whether seeking is cheap.  This is true for local files.
	 */
	gcc_pure
	bool CheapSeeking() const;

	/**
	 * Seeks to the specified position in the stream.  This will most
	 * likely fail if the "seekable" flag is false.
	 *
	 * The caller must lock the mutex.
	 *
	 * @param offset the relative offset
	 * @param whence the base of the seek, one of SEEK_SET, SEEK_CUR, SEEK_END
	 */
	bool Seek(offset_type offset, int whence, Error &error);

	/**
	 * Wrapper for Seek() which locks and unlocks the mutex; the
	 * caller must not be holding it already.
	 */
	bool LockSeek(offset_type offset, int whence, Error &error);

	/**
	 * Rewind to the beginning of the stream.  This is a wrapper
	 * for Seek(0, SEEK_SET, error).
	 */
	bool Rewind(Error &error);
	bool LockRewind(Error &error);

	/**
	 * Returns true if the stream has reached end-of-file.
	 *
	 * The caller must lock the mutex.
	 */
	gcc_pure
	bool IsEOF();

	/**
	 * Wrapper for IsEOF() which locks and unlocks the mutex; the
	 * caller must not be holding it already.
	 */
	gcc_pure
	bool LockIsEOF();

	/**
	 * Reads the tag from the stream.
	 *
	 * The caller must lock the mutex.
	 *
	 * @return a tag object which must be freed by the caller, or
	 * nullptr if the tag has not changed since the last call
	 */
	gcc_malloc
	Tag *ReadTag();

	/**
	 * Wrapper for ReadTag() which locks and unlocks the mutex;
	 * the caller must not be holding it already.
	 */
	gcc_malloc
	Tag *LockReadTag();

	/**
	 * Returns true if the next read operation will not block: either data
	 * is available, or end-of-stream has been reached, or an error has
	 * occurred.
	 *
	 * The caller must lock the mutex.
	 */
	gcc_pure
	bool IsAvailable();

	/**
	 * Reads data from the stream into the caller-supplied buffer.
	 * Returns 0 on error or eof (check with IsEOF()).
	 *
	 * The caller must lock the mutex.
	 *
	 * @param is the InputStream object
	 * @param ptr the buffer to read into
	 * @param size the maximum number of bytes to read
	 * @return the number of bytes read
	 */
	gcc_nonnull_all
	size_t Read(void *ptr, size_t size, Error &error);

	/**
	 * Wrapper for Read() which locks and unlocks the mutex;
	 * the caller must not be holding it already.
	 */
	gcc_nonnull_all
	size_t LockRead(void *ptr, size_t size, Error &error);
};

#endif
