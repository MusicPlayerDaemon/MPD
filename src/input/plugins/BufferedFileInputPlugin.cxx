/*
 * Copyright 2015-2018 Cary Audio
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
#include "BufferedFileInputPlugin.hxx"
#include "../InputStream.hxx"
#include "thread/Name.hxx"
#include "thread/Thread.hxx"
#include "thread/Cond.hxx"
#include "util/CircularBuffer.hxx"
#include "util/HugeAllocator.hxx"
#include "util/Domain.hxx"
#include "util/RuntimeError.hxx"
#include "Log.hxx"
#include "fs/io/FileReader.hxx"
#include "fs/FileInfo.hxx"

#include <assert.h>
#include <string.h>
#include <vector>
#include <unistd.h>

static constexpr Domain domain("BufferedFileInputStream");
static constexpr size_t max_block_size = 1 * 1024 * 1024;

class BufferedFileInputStream : public InputStream {
	FileReader reader;

	Thread thread;

	Cond buffer_cond;
	Cond seek_read_cond;
	Mutex buffer_mutex;

	std::exception_ptr postponed_exception;

	HugeArray<uint8_t> allocation;
	CircularBuffer<uint8_t> buffer;

	/**
	 * Shall the stream be closed?
	 */
	bool close = false;

	/**
	 * Has the end of the stream been seen by the thread?
	 */
	bool eof = false;
	bool flag_seek = false;

	size_t current_block_size = 1024;
	static constexpr size_t max_buffer_size = max_block_size;
	size_t resume_at_size;

public:
	BufferedFileInputStream(const char *path,
			  FileReader &&_reader, off_t _size, Mutex &_mutex, Cond &_cond,
			  size_t _buffer_size = 20*1024*1024)
		:InputStream(path, _mutex, _cond),
		 reader(std::move(_reader)),
		 thread(BIND_THIS_METHOD(ThreadFunc)),
		 allocation(_buffer_size),
		 buffer(&allocation.front(), allocation.size()),
		 resume_at_size(_buffer_size / 2)
	{
		size = _size;
		seekable = true;
		allocation.ForkCow(false);

		thread.Start();
		int try_cnt = 1000;
		while (try_cnt-- > 0 && !ready) {
			usleep(1000);
		}
	}

	~BufferedFileInputStream() {
		std::unique_lock<Mutex> lock(buffer_mutex);
		close = true;
		buffer.Clear();
		seek_read_cond.broadcast();
		buffer_cond.broadcast();
		lock.unlock();
		thread.Join();
	}

	/* virtual methods from InputStream */
	void Check() override final {
		assert(!thread.IsInside());

		if (postponed_exception)
			std::rethrow_exception(postponed_exception);
	}
	bool IsEOF() noexcept final {
		assert(!thread.IsInside());
		return (eof && buffer.empty());
	}
	size_t Read(void *ptr, size_t size) override final;
	void Seek(offset_type offset) override final;

	/**
	 * Read from the stream.
	 *
	 * The #InputStream is not locked.
	 *
	 * Throws std::runtime_error on error.
	 *
	 * @return 0 on end-of-file
	 */
	size_t ThreadRead(void *ptr, size_t size);


	/**
	 * Throws std::runtime_error on error.
	 */
	 void ClientSeek(offset_type &offset);

	bool BufferSeek(offset_type new_offset);

private:
	void ThreadFunc();
};

void
BufferedFileInputStream::ThreadFunc()
{
	FormatThreadName("input:%s", "BufferedFileInputStream");

	{
		std::unique_lock<Mutex> lock(mutex);
		/* we're ready, tell it to our client */
		SetReady();
	}

	std::vector<uint8_t> cpy_buffer;
	cpy_buffer.resize(max_buffer_size);
	std::unique_lock<Mutex> lock(buffer_mutex);
	while (!close) {
		assert(!postponed_exception);

		if (flag_seek) {
			flag_seek = false;
			buffer.Clear();
			ClientSeek(offset);
			seek_read_cond.broadcast();
			lock.unlock();
			lock.lock();
			continue;
		}

		if (eof || (buffer.GetSpace() < max_buffer_size)) {
			seek_read_cond.signal();
			buffer_cond.wait(buffer_mutex);
		} else {
			try {
				if (current_block_size < max_buffer_size) { // increase block size to speed up read
					current_block_size = std::min(current_block_size * 2, max_buffer_size);
				}
				auto range = buffer.Write();
				lock.unlock();
				auto nbytes = std::min(current_block_size, range.size);
				nbytes = ThreadRead(range.data, nbytes);
				lock.lock();
				buffer.Append(nbytes);
				if (nbytes == 0) {
					eof = true;
				}
				if (flag_seek) {
					continue;
				}
				seek_read_cond.signal();
			} catch (...) {
				postponed_exception = std::current_exception();
				seek_read_cond.broadcast();
				break;
			}
		}
	}
}

inline size_t
BufferedFileInputStream::Read(void *ptr, size_t read_size)
{
	assert(!thread.IsInside());

	size_t nbytes;
	{
		const ScopeUnlock unlock(mutex);
		size_t remain_size = read_size;
		uint8_t *pw = (uint8_t*)ptr;

		while (!close && remain_size>0 && !IsEOF()) {
			if (postponed_exception)
				std::rethrow_exception(postponed_exception);

			if (remain_size > 0) {
				auto range = buffer.Read();
				auto need_size = std::min(range.size, remain_size);
				memcpy(pw, range.data, need_size);
				pw += need_size;
				remain_size -= need_size;
				std::lock_guard<Mutex> lock(buffer_mutex);
				buffer.Consume(need_size);
			}
			if (remain_size > 0) {
				auto range = buffer.Read();
				auto need_size = std::min(range.size, remain_size);
				memcpy(pw, range.data, need_size);
				pw += need_size;
				remain_size -= need_size;
				std::lock_guard<Mutex> lock(buffer_mutex);
				buffer.Consume(need_size);
			}

			if (remain_size > 0) {
				std::lock_guard<Mutex> lock(buffer_mutex);
				buffer_cond.signal();
				seek_read_cond.wait(buffer_mutex);
			} else if (buffer.GetSize() <= resume_at_size) {
				//std::unique_lock<Mutex> lock(buffer_mutex);
				buffer_cond.signal();
				break;
			}
		}
		nbytes = read_size - remain_size;
	}
	offset += nbytes;

	return nbytes;
}

bool
BufferedFileInputStream::BufferSeek(offset_type new_offset)
{
	bool ret;
	offset_type start = offset;
	offset_type end = offset + buffer.GetSize();

	if (KnownSize() && new_offset != GetSize()) {
		eof = false;
		postponed_exception = nullptr;
	}
	if (start <= new_offset && new_offset <= end) {
		size_t remain_size = new_offset - start;
		if (remain_size) {
			auto r = buffer.Read();
			auto nbytes = std::min(remain_size, r.size);
			remain_size -= nbytes;
			buffer.Consume(nbytes);
		}
		if (remain_size) {
			auto r = buffer.Read();
			auto nbytes = std::min(remain_size, r.size);
			remain_size -= nbytes;
			buffer.Consume(nbytes);
		}
		ret = true;
	} else {
		ret = false;
	}

	return ret;
}

void
BufferedFileInputStream::Seek(offset_type new_offset)
{
	std::lock_guard<Mutex> lock(buffer_mutex);
	if (BufferSeek(new_offset)) {
		offset = new_offset;
		buffer_cond.signal();
		return;
	}
	flag_seek = true;
	offset = new_offset;
	buffer_cond.signal();
	const ScopeUnlock unlock(mutex);
	seek_read_cond.wait(buffer_mutex);
	if (postponed_exception)
		std::rethrow_exception(postponed_exception);
}

void
BufferedFileInputStream::ClientSeek(offset_type &new_offset)
try {
	reader.Seek((off_t)new_offset);
} catch (...) {
	postponed_exception = std::current_exception();
}

size_t
BufferedFileInputStream::ThreadRead(void *ptr, size_t read_size)
{
	size_t nbytes = reader.Read(ptr, read_size);
	return nbytes;
}

InputStreamPtr
OpenBufferedFileInputStream(Path path,
		    Mutex &mutex, Cond &cond)
{
	FileReader reader(path);

	const FileInfo info = reader.GetFileInfo();

	if (!info.IsRegular())
		throw FormatRuntimeError("Not a regular file: %s",
					 path.c_str());

#ifdef POSIX_FADV_SEQUENTIAL
	posix_fadvise(reader.GetFD().Get(), (off_t)0, info.GetSize(),
		      POSIX_FADV_SEQUENTIAL);
#endif

	return InputStreamPtr(new BufferedFileInputStream(path.ToUTF8().c_str(),
						  std::move(reader), info.GetSize(),
						  mutex, cond));
}
