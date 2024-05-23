// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include "Offset.hxx"
#include "Ptr.hxx"
#include "thread/Mutex.hxx"

#include <cassert>
#include <cstddef>
#include <memory>
#include <span>
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
	template<typename U>
	[[nodiscard]]
	InputStream(U &&_uri, Mutex &_mutex) noexcept
		:uri(std::forward<U>(_uri)),
		 mutex(_mutex)
	{
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
	[[nodiscard]]
	static InputStreamPtr Open(const char *uri, Mutex &mutex);

	/**
	 * Just like Open(), but waits for the stream to become ready.
	 * It is a wrapper for Open(), WaitReady() and Check().
	 */
	[[nodiscard]]
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
	[[nodiscard]]
	InputStreamHandler *ExchangeHandler(InputStreamHandler *new_handler) noexcept {
		return std::exchange(handler, new_handler);
	}

	/**
	 * The absolute URI which was used to open this stream.
	 *
	 * No lock necessary for this method.
	 */
	[[nodiscard]]
	const char *GetURI() const noexcept {
		return uri.c_str();
	}

	[[nodiscard]]
	std::string_view GetUriView() const noexcept {
		return uri;
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
	[[nodiscard]]
	bool IsReady() const {
		return ready;
	}

	[[nodiscard]] [[gnu::pure]]
	bool HasMimeType() const noexcept {
		assert(ready);

		return !mime.empty();
	}

	[[nodiscard]] [[gnu::pure]]
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

	[[nodiscard]] [[gnu::pure]]
	bool KnownSize() const noexcept {
		assert(ready);

		return size != UNKNOWN_SIZE;
	}

	[[nodiscard]] [[gnu::pure]]
	offset_type GetSize() const noexcept {
		assert(ready);
		assert(KnownSize());

		return size;
	}

	void AddOffset(offset_type delta) noexcept {
		assert(ready);

		offset += delta;
	}

	[[nodiscard]] [[gnu::pure]]
	offset_type GetOffset() const noexcept {
		assert(ready);

		return offset;
	}

	[[nodiscard]] [[gnu::pure]]
	offset_type GetRest() const noexcept {
		assert(ready);
		assert(KnownSize());

		return size - offset;
	}

	[[nodiscard]] [[gnu::pure]]
	bool IsSeekable() const noexcept {
		assert(ready);

		return seekable;
	}

	/**
	 * Determines whether seeking is cheap.  This is true for local files.
	 */
	[[nodiscard]] [[gnu::pure]]
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
		std::unique_lock lock{mutex};
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
	[[nodiscard]] [[gnu::pure]]
	virtual bool IsEOF() const noexcept = 0;

	/**
	 * Wrapper for IsEOF() which locks and unlocks the mutex; the
	 * caller must not be holding it already.
	 */
	[[nodiscard]] [[gnu::pure]]
	bool LockIsEOF() const noexcept;

	/**
	 * Reads the tag from the stream.
	 *
	 * The caller must lock the mutex.
	 *
	 * @return a tag object or nullptr if the tag has not changed
	 * since the last call
	 */
	[[nodiscard]]
	virtual std::unique_ptr<Tag> ReadTag() noexcept;

	/**
	 * Wrapper for ReadTag() which locks and unlocks the mutex;
	 * the caller must not be holding it already.
	 */
	[[nodiscard]]
	std::unique_ptr<Tag> LockReadTag() noexcept;

	/**
	 * Returns true if the next read operation will not block: either data
	 * is available, or end-of-stream has been reached, or an error has
	 * occurred.
	 *
	 * The caller must lock the mutex.
	 */
	[[nodiscard]] [[gnu::pure]]
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
	[[nodiscard]]
	virtual std::size_t Read(std::unique_lock<Mutex> &lock,
				 std::span<std::byte> dest) = 0;

	/**
	 * Wrapper for Read() which locks and unlocks the mutex;
	 * the caller must not be holding it already.
	 *
	 * Throws std::runtime_error on error.
	 */
	[[nodiscard]]
	std::size_t LockRead(std::span<std::byte> dest);

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
	void ReadFull(std::unique_lock<Mutex> &lock, std::span<std::byte> dest);

	/**
	 * Wrapper for ReadFull() which locks and unlocks the mutex;
	 * the caller must not be holding it already.
	 *
	 * Throws std::runtime_error on error.
	 */
	void LockReadFull(std::span<std::byte> dest);

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
	[[nodiscard]]
	ScopeExchangeInputStreamHandler(InputStream &_is,
					InputStreamHandler *new_handler) noexcept
		:is(_is), old_handler(is.ExchangeHandler(new_handler)) {}

	ScopeExchangeInputStreamHandler(const ScopeExchangeInputStreamHandler &) = delete;

	~ScopeExchangeInputStreamHandler() noexcept {
		is.SetHandler(old_handler);
	}
};
