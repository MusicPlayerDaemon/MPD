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
#include "../AsyncInputStream.hxx"
#include "../InputPlugin.hxx"
#include "lib/nfs/Domain.hxx"
#include "lib/nfs/Glue.hxx"
#include "lib/nfs/FileReader.hxx"
#include "util/HugeAllocator.hxx"
#include "util/StringUtil.hxx"
#include "util/Error.hxx"

extern "C" {
#include <nfsc/libnfs.h>
}

#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>

/**
 * Do not buffer more than this number of bytes.  It should be a
 * reasonable limit that doesn't make low-end machines suffer too
 * much, but doesn't cause stuttering on high-latency lines.
 */
static const size_t NFS_MAX_BUFFERED = 512 * 1024;

/**
 * Resume the stream at this number of bytes after it has been paused.
 */
static const size_t NFS_RESUME_AT = 384 * 1024;

class NfsInputStream final : public AsyncInputStream, NfsFileReader {
	uint64_t next_offset;

	bool reconnect_on_resume, reconnecting;

public:
	NfsInputStream(const char *_uri,
		       Mutex &_mutex, Cond &_cond,
		       void *_buffer)
		:AsyncInputStream(_uri, _mutex, _cond,
				  _buffer, NFS_MAX_BUFFERED,
				  NFS_RESUME_AT),
		 reconnect_on_resume(false), reconnecting(false) {}

	virtual ~NfsInputStream() {
		DeferClose();
	}

	bool Open(Error &error) {
		assert(!IsReady());

		return NfsFileReader::Open(GetURI(), error);
	}

private:
	bool DoRead();

protected:
	/* virtual methods from AsyncInputStream */
	virtual void DoResume() override;
	virtual void DoSeek(offset_type new_offset) override;

private:
	/* virtual methods from NfsFileReader */
	void OnNfsFileOpen(uint64_t size) override;
	void OnNfsFileRead(const void *data, size_t size) override;
	void OnNfsFileError(Error &&error) override;
};

bool
NfsInputStream::DoRead()
{
	assert(NfsFileReader::IsIdle());

	int64_t remaining = size - next_offset;
	if (remaining <= 0)
		return true;

	const size_t buffer_space = GetBufferSpace();
	if (buffer_space == 0) {
		Pause();
		return true;
	}

	size_t nbytes = std::min<size_t>(std::min<uint64_t>(remaining, 32768),
					 buffer_space);

	mutex.unlock();
	Error error;
	bool success = NfsFileReader::Read(next_offset, nbytes, error);
	mutex.lock();

	if (!success) {
		PostponeError(std::move(error));
		return false;
	}

	return true;
}

void
NfsInputStream::DoResume()
{
	if (reconnect_on_resume) {
		/* the NFS connection has died while this stream was
		   "paused" - attempt to reconnect */

		reconnect_on_resume = false;
		reconnecting = true;

		mutex.unlock();
		NfsFileReader::Close();

		Error error;
		bool success = NfsFileReader::Open(GetURI(), error);
		mutex.lock();

		if (!success) {
			postponed_error = std::move(error);
			cond.broadcast();
		}

		return;
	}

	assert(NfsFileReader::IsIdle());

	DoRead();
}

void
NfsInputStream::DoSeek(offset_type new_offset)
{
	mutex.unlock();
	NfsFileReader::CancelRead();
	mutex.lock();

	next_offset = offset = new_offset;
	SeekDone();
	DoRead();
}

void
NfsInputStream::OnNfsFileOpen(uint64_t _size)
{
	const ScopeLock protect(mutex);

	if (reconnecting) {
		/* reconnect has succeeded */

		reconnecting = false;
		DoRead();
		return;
	}

	size = _size;
	seekable = true;
	next_offset = 0;
	SetReady();
	DoRead();
}

void
NfsInputStream::OnNfsFileRead(const void *data, size_t data_size)
{
	const ScopeLock protect(mutex);
	assert(!IsBufferFull());
	assert(IsBufferFull() == (GetBufferSpace() == 0));
	AppendToBuffer(data, data_size);

	next_offset += data_size;

	DoRead();
}

void
NfsInputStream::OnNfsFileError(Error &&error)
{
	const ScopeLock protect(mutex);

	if (IsPaused()) {
		/* while we're paused, don't report this error to the
		   client just yet (it might just be timeout, maybe
		   playback has been paused for quite some time) -
		   wait until the stream gets resumed and try to
		   reconnect, to give it another chance */

		reconnect_on_resume = true;
		return;
	}

	postponed_error = std::move(error);

	if (IsSeekPending())
		SeekDone();
	else if (!IsReady())
		SetReady();
	else
		cond.broadcast();
}

/*
 * InputPlugin methods
 *
 */

static InputPlugin::InitResult
input_nfs_init(const config_param &, Error &)
{
	nfs_init();
	return InputPlugin::InitResult::SUCCESS;
}

static void
input_nfs_finish()
{
	nfs_finish();
}

static InputStream *
input_nfs_open(const char *uri,
	       Mutex &mutex, Cond &cond,
	       Error &error)
{
	if (!StringStartsWith(uri, "nfs://"))
		return nullptr;

	void *buffer = HugeAllocate(NFS_MAX_BUFFERED);
	if (buffer == nullptr) {
		error.Set(nfs_domain, "Out of memory");
		return nullptr;
	}

	NfsInputStream *is = new NfsInputStream(uri, mutex, cond, buffer);
	if (!is->Open(error)) {
		delete is;
		return nullptr;
	}

	return is;
}

const InputPlugin input_plugin_nfs = {
	"nfs",
	input_nfs_init,
	input_nfs_finish,
	input_nfs_open,
};
