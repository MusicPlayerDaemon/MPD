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
#include "Offset.hxx"
#include "thread/Mutex.hxx"
#include "Compiler.h"

#include <string>

#include <assert.h>
#include <stdint.h>

class Cond;
class Error;
struct Tag;

class InputStream {
public:
	typedef ::offset_type offset_type;

private:
	/**
	 * The absolute URI which was used to open this stream.
	 */
	std::string uri;

public:
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

protected:
	/**
	 * indicates whether the stream is ready for reading and
	 * whether the other attributes in this struct are valid
	 */
	bool ready;

	/**
	 * if true, then the stream is fully seekable
	 */
	bool seekable;

	static constexpr offset_type UNKNOWN_SIZE = -1;

	/**
	 * the size of the resource, or #UNKNOWN_SIZE if unknown
	 */
	offset_type size;

	/**
	 * the current offset within the stream
	 */
	offset_type offset;

private:
	/**
	 * the MIME content type of the resource, or empty if unknown.
	 */
	std::string mime;

public:
	InputStream(const char *_uri, Mutex &_mutex, Cond &_cond)
		:uri(_uri),
		 mutex(_mutex), cond(_cond),
		 ready(false), seekable(false),
		 size(UNKNOWN_SIZE), offset(0) {
		assert(_uri != nullptr);
	}

	/**
	 * Close the input stream and free resources.
	 *
	 * The caller must not lock the mutex.
	 */
	virtual ~InputStream();

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
	 * The absolute URI which was used to open this stream.
	 *
	 * No lock necessary for this method.
	 */
	const char *GetURI() const {
		return uri.c_str();
	}

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
	virtual bool Check(Error &error);

	/**
	 * Update the public attributes.  Call before accessing attributes
	 * such as "ready" or "offset".
	 */
	virtual void Update();

	void SetReady();

	/**
	 * Return whether the stream is ready for reading and whether
	 * the other attributes in this struct are valid.
	 *
	 * The caller must lock the mutex.
	 */
	bool IsReady() const {
		return ready;
	}

	void WaitReady();

	/**
	 * Wrapper for WaitReady() which locks and unlocks the mutex;
	 * the caller must not be holding it already.
	 */
	void LockWaitReady();

	gcc_pure
	bool HasMimeType() const {
		assert(ready);

		return !mime.empty();
	}

	gcc_pure
	const char *GetMimeType() const {
		assert(ready);

		return mime.empty() ? nullptr : mime.c_str();
	}

	void ClearMimeType() {
		mime.clear();
	}

	gcc_nonnull_all
	void SetMimeType(const char *_mime) {
		assert(!ready);

		mime = _mime;
	}

	void SetMimeType(std::string &&_mime) {
		assert(!ready);

		mime = std::move(_mime);
	}

	gcc_nonnull_all
	void OverrideMimeType(const char *_mime) {
		assert(ready);

		mime = _mime;
	}

	gcc_pure
	bool KnownSize() const {
		assert(ready);

		return size != UNKNOWN_SIZE;
	}

	gcc_pure
	offset_type GetSize() const {
		assert(ready);
		assert(KnownSize());

		return size;
	}

	void AddOffset(offset_type delta) {
		assert(ready);

		offset += delta;
	}

	gcc_pure
	offset_type GetOffset() const {
		assert(ready);

		return offset;
	}

	gcc_pure
	offset_type GetRest() const {
		assert(ready);
		assert(KnownSize());

		return size - offset;
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
	 */
	virtual bool Seek(offset_type offset, Error &error);

	/**
	 * Wrapper for Seek() which locks and unlocks the mutex; the
	 * caller must not be holding it already.
	 */
	bool LockSeek(offset_type offset, Error &error);

	/**
	 * Rewind to the beginning of the stream.  This is a wrapper
	 * for Seek(0, error).
	 */
	bool Rewind(Error &error) {
		return Seek(0, error);
	}

	bool LockRewind(Error &error) {
		return LockSeek(0, error);
	}

	/**
	 * Returns true if the stream has reached end-of-file.
	 *
	 * The caller must lock the mutex.
	 */
	gcc_pure
	virtual bool IsEOF() = 0;

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
	virtual Tag *ReadTag();

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
	virtual bool IsAvailable();

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
	virtual size_t Read(void *ptr, size_t size, Error &error) = 0;

	/**
	 * Wrapper for Read() which locks and unlocks the mutex;
	 * the caller must not be holding it already.
	 */
	gcc_nonnull_all
	size_t LockRead(void *ptr, size_t size, Error &error);
};

#endif
