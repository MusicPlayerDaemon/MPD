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

#include "NfsInputPlugin.hxx"
#include "../AsyncInputStream.hxx"
#include "../InputPlugin.hxx"
#include "lib/nfs/Glue.hxx"
#include "lib/nfs/FileReader.hxx"

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

class NfsInputStream final : NfsFileReader, public AsyncInputStream {
	uint64_t next_offset;

	bool reconnect_on_resume = false, reconnecting = false;

public:
	NfsInputStream(const char *_uri, Mutex &_mutex)
		:AsyncInputStream(NfsFileReader::GetEventLoop(),
				  _uri, _mutex,
				  NFS_MAX_BUFFERED,
				  NFS_RESUME_AT) {}

	~NfsInputStream() override {
		DeferClose();
	}

	NfsInputStream(const NfsInputStream &) = delete;
	NfsInputStream &operator=(const NfsInputStream &) = delete;

	void Open() {
		assert(!IsReady());

		NfsFileReader::Open(GetURI());
	}

private:
	void DoRead();

protected:
	/* virtual methods from AsyncInputStream */
	void DoResume() override;
	void DoSeek(offset_type new_offset) override;

private:
	/* virtual methods from NfsFileReader */
	void OnNfsFileOpen(uint64_t size) noexcept override;
	void OnNfsFileRead(const void *data, size_t size) noexcept override;
	void OnNfsFileError(std::exception_ptr &&e) noexcept override;
};

void
NfsInputStream::DoRead()
{
	assert(NfsFileReader::IsIdle());

	int64_t remaining = size - next_offset;
	if (remaining <= 0)
		return;

	const size_t buffer_space = GetBufferSpace();
	if (buffer_space == 0) {
		Pause();
		return;
	}

	size_t nbytes = std::min<size_t>(std::min<uint64_t>(remaining, 32768),
					 buffer_space);

	try {
		const ScopeUnlock unlock(mutex);
		NfsFileReader::Read(next_offset, nbytes);
	} catch (...) {
		postponed_exception = std::current_exception();
		InvokeOnAvailable();
	}
}

void
NfsInputStream::DoResume()
{
	if (reconnect_on_resume) {
		/* the NFS connection has died while this stream was
		   "paused" - attempt to reconnect */

		reconnect_on_resume = false;
		reconnecting = true;

		ScopeUnlock unlock(mutex);

		NfsFileReader::Close();
		NfsFileReader::Open(GetURI());

		return;
	}

	assert(NfsFileReader::IsIdle());

	DoRead();
}

void
NfsInputStream::DoSeek(offset_type new_offset)
{
	{
		const ScopeUnlock unlock(mutex);
		NfsFileReader::CancelRead();
	}

	next_offset = offset = new_offset;
	SeekDone();
	DoRead();
}

void
NfsInputStream::OnNfsFileOpen(uint64_t _size) noexcept
{
	const std::scoped_lock<Mutex> protect(mutex);

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
NfsInputStream::OnNfsFileRead(const void *data, size_t data_size) noexcept
{
	const std::scoped_lock<Mutex> protect(mutex);
	assert(!IsBufferFull());
	assert(IsBufferFull() == (GetBufferSpace() == 0));
	AppendToBuffer(data, data_size);

	next_offset += data_size;

	DoRead();
}

void
NfsInputStream::OnNfsFileError(std::exception_ptr &&e) noexcept
{
	const std::scoped_lock<Mutex> protect(mutex);

	if (IsPaused()) {
		/* while we're paused, don't report this error to the
		   client just yet (it might just be timeout, maybe
		   playback has been paused for quite some time) -
		   wait until the stream gets resumed and try to
		   reconnect, to give it another chance */

		reconnect_on_resume = true;
		return;
	}

	postponed_exception = std::move(e);

	if (IsSeekPending())
		SeekDone();
	else if (!IsReady())
		SetReady();
	else
		InvokeOnAvailable();
}

/*
 * InputPlugin methods
 *
 */

static void
input_nfs_init(EventLoop &event_loop, const ConfigBlock &)
{
	nfs_init(event_loop);
}

static void
input_nfs_finish() noexcept
{
	nfs_finish();
}

static InputStreamPtr
input_nfs_open(const char *uri,
	       Mutex &mutex)
{
	auto is = std::make_unique<NfsInputStream>(uri, mutex);
	is->Open();
	return is;
}

static constexpr const char *nfs_prefixes[] = {
	"nfs://",
	nullptr
};

const InputPlugin input_plugin_nfs = {
	"nfs",
	nfs_prefixes,
	input_nfs_init,
	input_nfs_finish,
	input_nfs_open,
	nullptr
};
