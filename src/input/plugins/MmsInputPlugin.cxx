// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "MmsInputPlugin.hxx"
#include "input/ThreadInputStream.hxx"
#include "input/InputPlugin.hxx"
#include "system/Error.hxx"

#include <libmms/mmsx.h>

#include <stdexcept>

static constexpr size_t MMS_BUFFER_SIZE = 256 * 1024;

class MmsInputStream final : public ThreadInputStream {
	mmsx_t *mms;

public:
	MmsInputStream(const char *_uri, Mutex &_mutex)
		:ThreadInputStream(input_plugin_mms.name, _uri, _mutex,
				   MMS_BUFFER_SIZE) {
	}

	~MmsInputStream() noexcept override {
		Stop();
	}

	MmsInputStream(const MmsInputStream &) = delete;
	MmsInputStream &operator=(const MmsInputStream &) = delete;

protected:
	void Open() override;
	size_t ThreadRead(void *ptr, size_t size) override;

	void Close() noexcept override {
		mmsx_close(mms);
	}
};

void
MmsInputStream::Open()
{
	{
		const ScopeUnlock unlock(mutex);

		mms = mmsx_connect(nullptr, nullptr, GetURI(), 128 * 1024);
		if (mms == nullptr)
			throw std::runtime_error("mmsx_connect() failed");
	}

	/* TODO: is this correct?  at least this selects the ffmpeg
	   decoder, which seems to work fine */
	SetMimeType("audio/x-ms-wma");
}

static InputStreamPtr
input_mms_open(const char *url,
	       Mutex &mutex)
{
	auto m = std::make_unique<MmsInputStream>(url, mutex);
	m->Start();
	return m;
}

size_t
MmsInputStream::ThreadRead(void *ptr, size_t read_size)
{
	/* unfortunately, mmsx_read() blocks until the whole buffer
	   has been filled; to avoid big latencies, limit the size of
	   each chunk we read to a reasonable size */
	constexpr size_t MAX_CHUNK = 16384;
	if (read_size > MAX_CHUNK)
		read_size = MAX_CHUNK;

	int nbytes = mmsx_read(nullptr, mms, (char *)ptr, read_size);
	if (nbytes <= 0) {
		if (nbytes < 0)
			throw MakeErrno("mmsx_read() failed");
		return 0;
	}

	return (size_t)nbytes;
}

static constexpr const char *mms_prefixes[] = {
	"mms://",
	"mmsh://",
	"mmst://",
	"mmsu://",
	nullptr
};

const InputPlugin input_plugin_mms = {
	"mms",
	mms_prefixes,
	nullptr,
	nullptr,
	input_mms_open,
	nullptr
};
