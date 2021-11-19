/*
 * Copyright 2003-2021 The Music Player Daemon Project
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

#include "Offset.hxx"
#include "Ptr.hxx"
#include "thread/Mutex.hxx"

#include <cassert>
#include <memory>
#include <string>
#include <utility>

struct Tag;
class InputStreamHandler;

class InputStream {
public:
	typedef ::offset_type offset_type;

private:
	/**
	 * The absolute URI which was used to open this stream.
	 */
	const std::string uri;

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

private:
	/**
	 * An (optional) object which gets receives events from this
	 * #InputStream.
	 *
	 * This object is allocated by the client, and the client is
	 * responsible for freeing it.
	 */
	InputStreamHandler *handler = nullptr;

protected:
	/**
	 * indicates whether the stream is ready for reading and
	 * whether the other attributes in this struct are valid
	 */
	bool ready = false;

	/**
	 * if true, then the stream is fully seekable
	 */
	bool seekable = false;

	static constexpr offset_type UNKNOWN_SIZE = ~offset_type(0);

	/**
	 * the size of the resource, or #UNKNOWN_SIZE if unknown
	 */
	offset_type size = UNKNOWN_SIZE;

	/**
	 * the current offset within the stream
	 */
	offset_type offset = 0;

private:
	/**
	 * the MIME content type of the resource, or empty if unknown.
	 */
	std::string mime;

public:
	InputStream(const char *_uri, Mutex &_mutex) noexcept
		:uri(_uri),
		 mutex(_mutex) {
		assert(_uri != nullptr);
	}

	/**
	 * Close the input stream and free resources.
	 *
	 * The caller must not lock the mutex.
	 */
	virtual ~InputStream() noexcept;

	InputStream(const InputStream &) = delete;
	InputStream &operator=(const InputStream &) = delete;

	/**
	 * Opens a new input stream.  You may not access it until the "ready"
	 * flag is set.
	 *
	 * Throws std::runtime_error on error.
	 *
	 * @param mutex a mutex that is used to protect this object; must be
	 * locked before calling any of the public methods
	 * @param cond a cond that gets signalled when the state of
	 * this object changes; may be nullptr if the caller doesn't want to get
	 * notifications
	 * @return an #InputStream object on success
	 */
	static InputStreamPtr Open(const char *uri, Mutex &mutex);

	/**
	 * Just like Open(), but waits for the stream to become ready.
	 * It is a wrapper for Open(), WaitReady() and Check().
	 */
	static InputStreamPtr OpenReady(const char *uri, Mutex &mutex);

	/**
	 * Install a new handler.
	 *
	 * The caller must lock the mutex.
	 */
	void SetHandler(InputStreamHandler *new_handler) noexcept {
		handler = new_handler;
	}

	/**
	 * Install a new handler and return the old one.
	 *
	 * The caller must lock the mutex.
	 */
	InputStreamHandler *ExchangeHandler(InputStreamHandler *new_handler) noexcept {
		return std::exchange(handler, new_handler);
	}

	/**
	 * The absolute URI which was used to open this stream.
	 *
	 * No lock necessary for this method.
	 */
	const char *GetURI() const noexcept {
		return uri.c_str();
	}

	/**
	 * Check for errors that may have occurred in the I/O thread.
	 * Throws std::runtime_error on error.
	 */
	virtual void Check();

	/**
	 * Update the public attributes.  Call before accessing attributes
	 * such as "ready" or "offset".
	 */
	virtual void Update() noexcept;

	void SetReady() noexcept;

	/**
	 * Return whether the stream is ready for reading and whether
	 * the other attributes in this struct are valid.
	 *
	 * The caller must lock the mutex.
	 */
	bool IsReady() const {
		return ready;
	}

	[[gnu::pure]]
	bool HasMimeType() const noexcept {
		assert(ready);

		return !mime.empty();
	}

	[[gnu::pure]]
	const char *GetMimeType() const noexcept {
		assert(ready);

		return mime.empty() ? nullptr : mime.c_str();
	}

	void ClearMimeType() noexcept {
		mime.clear();
	}

	[[gnu::nonnull]]
	void SetMimeType(const char *_mime) noexcept {
		assert(!ready);

		mime = _mime;
	}

	void SetMimeType(std::string &&_mime) noexcept {
		assert(!ready);

		mime = std::move(_mime);
	}

	[[gnu::pure]]
	bool KnownSize() const noexcept {
		assert(ready);

		return size != UNKNOWN_SIZE;
	}

	[[gnu::pure]]
	offset_type GetSize() const noexcept {
		assert(ready);
		assert(KnownSize());

		return size;
	}

	void AddOffset(offset_type delta) noexcept {
		assert(ready);

		offset += delta;
	}

	[[gnu::pure]]
	offset_type GetOffset() const noexcept {
		assert(ready);

		return offset;
	}

	[[gnu::pure]]
	offset_type GetRest() const noexcept {
		assert(ready);
		assert(KnownSize());

		return size - offset;
	}

	[[gnu::pure]]
	bool IsSeekable() const noexcept {
		assert(ready);

		return seekable;
	}

	/**
	 * Determines whether seeking is cheap.  This is true for local files.
	 */
	[[gnu::pure]]
	bool CheapSeeking() const noexcept;

	/**
	 * Seeks to the specified position in the stream.  This will most
	 * likely fail if the "seekable" flag is false.
	 *
	 * The caller must lock the mutex.
	 *
	 * Throws std::runtime_error on error.
	 *
	 * @param lock the locked mutex; may be used to wait on
	 * condition variables
	 * @param offset the relative offset
	 */
	virtual void Seek(std::unique_lock<Mutex> &lock, offset_type offset);

	/**
	 * Wrapper for Seek() which locks and unlocks the mutex; the
	 * caller must not be holding it already.
	 */
	void LockSeek(offset_type offset);

	/**
	 * Rewind to the beginning of the stream.  This is a wrapper
	 * for Seek(0, error).
	 */
	void Rewind(std::unique_lock<Mutex> &lock) {
		if (offset > 0)
			Seek(lock, 0);
	}

	void LockRewind() {
		std::unique_lock<Mutex> lock(mutex);
		Rewind(lock);
	}

	/**
	 * Skip input bytes.
	 */
	void Skip(std::unique_lock<Mutex> &lock,
		  offset_type _offset) {
		Seek(lock, GetOffset() + _offset);
	}

	void LockSkip(offset_type _offset);

	/**
	 * Returns true if the stream has reached end-of-file.
	 *
	 * The caller must lock the mutex.
	 */
	[[gnu::pure]]
	virtual bool IsEOF() const noexcept = 0;

	/**
	 * Wrapper for IsEOF() which locks and unlocks the mutex; the
	 * caller must not be holding it already.
	 */
	[[gnu::pure]]
	bool LockIsEOF() const noexcept;

	/**
	 * Reads the tag from the stream.
	 *
	 * The caller must lock the mutex.
	 *
	 * @return a tag object or nullptr if the tag has not changed
	 * since the last call
	 */
	virtual std::unique_ptr<Tag> ReadTag() noexcept;

	/**
	 * Wrapper for ReadTag() which locks and unlocks the mutex;
	 * the caller must not be holding it already.
	 */
	std::unique_ptr<Tag> LockReadTag() noexcept;

	/**
	 * Returns true if the next read operation will not block: either data
	 * is available, or end-of-stream has been reached, or an error has
	 * occurred.
	 *
	 * The caller must lock the mutex.
	 */
	[[gnu::pure]]
	virtual bool IsAvailable() const noexcept;

	/**
	 * Reads data from the stream into the caller-supplied buffer.
	 * Returns 0 on error or eof (check with IsEOF()).
	 *
	 * The caller must lock the mutex.
	 *
	 * Throws std::runtime_error on error.
	 *
	 * @param lock the locked mutex; may be used to wait on
	 * condition variables
	 * @param ptr the buffer to read into
	 * @param size the maximum number of bytes to read
	 * @return the number of bytes read
	 */
	[[gnu::nonnull]]
	virtual size_t Read(std::unique_lock<Mutex> &lock,
			    void *ptr, size_t size) = 0;

	/**
	 * Wrapper for Read() which locks and unlocks the mutex;
	 * the caller must not be holding it already.
	 *
	 * Throws std::runtime_error on error.
	 */
	[[gnu::nonnull]]
	size_t LockRead(void *ptr, size_t size);

	/**
	 * Reads the whole data from the stream into the caller-supplied buffer.
	 *
	 * The caller must lock the mutex.
	 *
	 * Throws std::runtime_error on error.
	 *
	 * @param ptr the buffer to read into
	 * @param size the number of bytes to read
	 * @return true if the whole data was read, false otherwise.
	 */
	[[gnu::nonnull]]
	void ReadFull(std::unique_lock<Mutex> &lock, void *ptr, size_t size);

	/**
	 * Wrapper for ReadFull() which locks and unlocks the mutex;
	 * the caller must not be holding it already.
	 *
	 * Throws std::runtime_error on error.
	 */
	[[gnu::nonnull]]
	void LockReadFull(void *ptr, size_t size);

protected:
	void InvokeOnReady() noexcept;
	void InvokeOnAvailable() noexcept;
};

/**
 * Install an #InputStreamHandler during the scope in which this
 * variable lives, and restore the old handler afterwards.
 */
class ScopeExchangeInputStreamHandler {
	InputStream &is;
	InputStreamHandler *const old_handler;

public:
	ScopeExchangeInputStreamHandler(InputStream &_is,
					InputStreamHandler *new_handler) noexcept
		:is(_is), old_handler(is.ExchangeHandler(new_handler)) {}

	ScopeExchangeInputStreamHandler(const ScopeExchangeInputStreamHandler &) = delete;

	~ScopeExchangeInputStreamHandler() noexcept {
		is.SetHandler(old_handler);
	}
};

#endif
