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
#include "CurlInputPlugin.hxx"
#include "../IcyInputStream.hxx"
#include "../InputStream.hxx"
#include "../InputPlugin.hxx"
#include "config/ConfigGlobal.hxx"
#include "config/ConfigData.hxx"
#include "tag/Tag.hxx"
#include "tag/TagBuilder.hxx"
#include "event/SocketMonitor.hxx"
#include "event/TimeoutMonitor.hxx"
#include "event/Call.hxx"
#include "IOThread.hxx"
#include "util/ASCII.hxx"
#include "util/CharUtil.hxx"
#include "util/NumberParser.hxx"
#include "util/CircularBuffer.hxx"
#include "util/HugeAllocator.hxx"
#include "util/Error.hxx"
#include "util/Domain.hxx"
#include "Log.hxx"

#include <assert.h>

#if defined(WIN32)
	#include <winsock2.h>
#else
	#include <sys/select.h>
#endif

#include <string.h>

#include <curl/curl.h>

#if LIBCURL_VERSION_NUM < 0x071200
#error libcurl is too old
#endif

/**
 * Do not buffer more than this number of bytes.  It should be a
 * reasonable limit that doesn't make low-end machines suffer too
 * much, but doesn't cause stuttering on high-latency lines.
 */
static const size_t CURL_MAX_BUFFERED = 512 * 1024;

/**
 * Resume the stream at this number of bytes after it has been paused.
 */
static const size_t CURL_RESUME_AT = 384 * 1024;

struct CurlInputStream final : public InputStream {
	/* some buffers which were passed to libcurl, which we have
	   too free */
	char range[32];
	struct curl_slist *request_headers;

	/** the curl handles */
	CURL *easy;

	/**
	 * A buffer where input_curl_writefunction() appends
	 * to, and input_curl_read() reads from.
	 */
	CircularBuffer<uint8_t> buffer;

	/**
	 * Is the connection currently paused?  That happens when the
	 * buffer was getting too large.  It will be unpaused when the
	 * buffer is below the threshold again.
	 */
	bool paused;

	/** error message provided by libcurl */
	char error_buffer[CURL_ERROR_SIZE];

	/** parser for icy-metadata */
	IcyInputStream *icy;

	/** the tag object ready to be requested via
	    InputStream::ReadTag() */
	Tag *tag;

	Error postponed_error;

	CurlInputStream(const char *_url, Mutex &_mutex, Cond &_cond,
			void *_buffer)
		:InputStream(_url, _mutex, _cond),
		 request_headers(nullptr),
		 buffer((uint8_t *)_buffer, CURL_MAX_BUFFERED),
		 paused(false),
		 icy(new IcyInputStream(this)),
		 tag(nullptr) {}

	~CurlInputStream();

	CurlInputStream(const CurlInputStream &) = delete;
	CurlInputStream &operator=(const CurlInputStream &) = delete;

	static InputStream *Open(const char *url, Mutex &mutex, Cond &cond,
				 Error &error);

	bool InitEasy(Error &error);

	/**
	 * Frees the current "libcurl easy" handle, and everything
	 * associated with it.
	 *
	 * Runs in the I/O thread.
	 */
	void FreeEasy();

	/**
	 * Frees the current "libcurl easy" handle, and everything associated
	 * with it.
	 *
	 * The mutex must not be locked.
	 */
	void FreeEasyIndirect();

	void HeaderReceived(const char *name, std::string &&value);

	size_t DataReceived(const void *ptr, size_t size);

	void Resume();
	bool FillBuffer(Error &error);

	/**
	 * Returns the number of bytes stored in the buffer.
	 *
	 * The caller must lock the mutex.
	 */
	gcc_pure
	size_t GetTotalBufferSize() const {
		return buffer.GetSize();
	}

	/**
	 * A HTTP request is finished.
	 *
	 * Runs in the I/O thread.  The caller must not hold locks.
	 */
	void RequestDone(CURLcode result, long status);

	/* virtual methods from InputStream */
	bool Check(Error &error) override;

	bool IsEOF() override {
		return easy == nullptr && buffer.IsEmpty();
	}

	Tag *ReadTag() override;

	bool IsAvailable() override {
		return postponed_error.IsDefined() || easy == nullptr ||
			!buffer.IsEmpty();
	}

	size_t Read(void *ptr, size_t size, Error &error) override;
	bool Seek(offset_type offset, int whence, Error &error) override;
};

class CurlMulti;

/**
 * Monitor for one socket created by CURL.
 */
class CurlSocket final : SocketMonitor {
	CurlMulti &multi;

public:
	CurlSocket(CurlMulti &_multi, EventLoop &_loop, int _fd)
		:SocketMonitor(_fd, _loop), multi(_multi) {}

	~CurlSocket() {
		/* TODO: sometimes, CURL uses CURL_POLL_REMOVE after
		   closing the socket, and sometimes, it uses
		   CURL_POLL_REMOVE just to move the (still open)
		   connection to the pool; in the first case,
		   Abandon() would be most appropriate, but it breaks
		   the second case - is that a CURL bug?  is there a
		   better solution? */
	}

	/**
	 * Callback function for CURLMOPT_SOCKETFUNCTION.
	 */
	static int SocketFunction(CURL *easy,
				  curl_socket_t s, int action,
				  void *userp, void *socketp);

	virtual bool OnSocketReady(unsigned flags) override;

private:
	static constexpr int FlagsToCurlCSelect(unsigned flags) {
		return (flags & (READ | HANGUP) ? CURL_CSELECT_IN : 0) |
			(flags & WRITE ? CURL_CSELECT_OUT : 0) |
			(flags & ERROR ? CURL_CSELECT_ERR : 0);
	}

	gcc_const
	static unsigned CurlPollToFlags(int action) {
		switch (action) {
		case CURL_POLL_NONE:
			return 0;

		case CURL_POLL_IN:
			return READ;

		case CURL_POLL_OUT:
			return WRITE;

		case CURL_POLL_INOUT:
			return READ|WRITE;
		}

		assert(false);
		gcc_unreachable();
	}
};

/**
 * Manager for the global CURLM object.
 */
class CurlMulti final : private TimeoutMonitor {
	CURLM *const multi;

public:
	CurlMulti(EventLoop &_loop, CURLM *_multi);

	~CurlMulti() {
		curl_multi_cleanup(multi);
	}

	bool Add(CurlInputStream *c, Error &error);
	void Remove(CurlInputStream *c);

	/**
	 * Check for finished HTTP responses.
	 *
	 * Runs in the I/O thread.  The caller must not hold locks.
	 */
	void ReadInfo();

	void Assign(curl_socket_t fd, CurlSocket &cs) {
		curl_multi_assign(multi, fd, &cs);
	}

	void SocketAction(curl_socket_t fd, int ev_bitmask);

	void InvalidateSockets() {
		SocketAction(CURL_SOCKET_TIMEOUT, 0);
	}

	/**
	 * This is a kludge to allow pausing/resuming a stream with
	 * libcurl < 7.32.0.  Read the curl_easy_pause manpage for
	 * more information.
	 */
	void ResumeSockets() {
		int running_handles;
		curl_multi_socket_all(multi, &running_handles);
	}

private:
	static int TimerFunction(CURLM *multi, long timeout_ms, void *userp);

	virtual void OnTimeout() override;
};

/**
 * libcurl version number encoded in a 24 bit integer.
 */
static unsigned curl_version_num;

/** libcurl should accept "ICY 200 OK" */
static struct curl_slist *http_200_aliases;

/** HTTP proxy settings */
static const char *proxy, *proxy_user, *proxy_password;
static unsigned proxy_port;

static CurlMulti *curl_multi;

static constexpr Domain http_domain("http");
static constexpr Domain curl_domain("curl");
static constexpr Domain curlm_domain("curlm");

CurlMulti::CurlMulti(EventLoop &_loop, CURLM *_multi)
	:TimeoutMonitor(_loop), multi(_multi)
{
	curl_multi_setopt(multi, CURLMOPT_SOCKETFUNCTION,
			  CurlSocket::SocketFunction);
	curl_multi_setopt(multi, CURLMOPT_SOCKETDATA, this);

	curl_multi_setopt(multi, CURLMOPT_TIMERFUNCTION, TimerFunction);
	curl_multi_setopt(multi, CURLMOPT_TIMERDATA, this);
}

/**
 * Find a request by its CURL "easy" handle.
 *
 * Runs in the I/O thread.  No lock needed.
 */
gcc_pure
static CurlInputStream *
input_curl_find_request(CURL *easy)
{
	assert(io_thread_inside());

	void *p;
	CURLcode code = curl_easy_getinfo(easy, CURLINFO_PRIVATE, &p);
	if (code != CURLE_OK)
		return nullptr;

	return (CurlInputStream *)p;
}

inline void
CurlInputStream::Resume()
{
	assert(io_thread_inside());

	if (paused) {
		paused = false;
		curl_easy_pause(easy, CURLPAUSE_CONT);

		if (curl_version_num < 0x072000)
			/* libcurl older than 7.32.0 does not update
			   its sockets after curl_easy_pause(); force
			   libcurl to do it now */
			curl_multi->ResumeSockets();

		curl_multi->InvalidateSockets();
	}
}

int
CurlSocket::SocketFunction(gcc_unused CURL *easy,
			   curl_socket_t s, int action,
			   void *userp, void *socketp) {
	CurlMulti &multi = *(CurlMulti *)userp;
	CurlSocket *cs = (CurlSocket *)socketp;

	assert(io_thread_inside());

	if (action == CURL_POLL_REMOVE) {
		delete cs;
		return 0;
	}

	if (cs == nullptr) {
		cs = new CurlSocket(multi, io_thread_get(), s);
		multi.Assign(s, *cs);
	} else {
#ifdef USE_EPOLL
		/* when using epoll, we need to unregister the socket
		   each time this callback is invoked, because older
		   CURL versions may omit the CURL_POLL_REMOVE call
		   when the socket has been closed and recreated with
		   the same file number (bug found in CURL 7.26, CURL
		   7.33 not affected); in that case, epoll refuses the
		   EPOLL_CTL_MOD because it does not know the new
		   socket yet */
		cs->Cancel();
#endif
	}

	unsigned flags = CurlPollToFlags(action);
	if (flags != 0)
		cs->Schedule(flags);
	return 0;
}

bool
CurlSocket::OnSocketReady(unsigned flags)
{
	assert(io_thread_inside());

	multi.SocketAction(Get(), FlagsToCurlCSelect(flags));
	return true;
}

/**
 * Runs in the I/O thread.  No lock needed.
 */
inline bool
CurlMulti::Add(CurlInputStream *c, Error &error)
{
	assert(io_thread_inside());
	assert(c != nullptr);
	assert(c->easy != nullptr);

	CURLMcode mcode = curl_multi_add_handle(multi, c->easy);
	if (mcode != CURLM_OK) {
		error.Format(curlm_domain, mcode,
			     "curl_multi_add_handle() failed: %s",
			     curl_multi_strerror(mcode));
		return false;
	}

	InvalidateSockets();
	return true;
}

/**
 * Call input_curl_easy_add() in the I/O thread.  May be called from
 * any thread.  Caller must not hold a mutex.
 */
static bool
input_curl_easy_add_indirect(CurlInputStream *c, Error &error)
{
	assert(c != nullptr);
	assert(c->easy != nullptr);

	bool result;
	BlockingCall(io_thread_get(), [c, &error, &result](){
			result = curl_multi->Add(c, error);
		});
	return result;
}

inline void
CurlMulti::Remove(CurlInputStream *c)
{
	curl_multi_remove_handle(multi, c->easy);
}

void
CurlInputStream::FreeEasy()
{
	assert(io_thread_inside());

	if (easy == nullptr)
		return;

	curl_multi->Remove(this);

	curl_easy_cleanup(easy);
	easy = nullptr;

	curl_slist_free_all(request_headers);
	request_headers = nullptr;
}

void
CurlInputStream::FreeEasyIndirect()
{
	BlockingCall(io_thread_get(), [this](){
			FreeEasy();
			curl_multi->InvalidateSockets();
		});

	assert(easy == nullptr);
}

inline void
CurlInputStream::RequestDone(CURLcode result, long status)
{
	assert(io_thread_inside());
	assert(!postponed_error.IsDefined());

	FreeEasy();

	const ScopeLock protect(mutex);

	if (result != CURLE_OK) {
		postponed_error.Format(curl_domain, result,
				       "curl failed: %s", error_buffer);
	} else if (status < 200 || status >= 300) {
		postponed_error.Format(http_domain, status,
				       "got HTTP status %ld",
				       status);
	}

	SetReady();
}

static void
input_curl_handle_done(CURL *easy_handle, CURLcode result)
{
	CurlInputStream *c = input_curl_find_request(easy_handle);
	assert(c != nullptr);

	long status = 0;
	curl_easy_getinfo(easy_handle, CURLINFO_RESPONSE_CODE, &status);

	c->RequestDone(result, status);
}

void
CurlMulti::SocketAction(curl_socket_t fd, int ev_bitmask)
{
	int running_handles;
	CURLMcode mcode = curl_multi_socket_action(multi, fd, ev_bitmask,
						   &running_handles);
	if (mcode != CURLM_OK)
		FormatError(curlm_domain,
			    "curl_multi_socket_action() failed: %s",
			    curl_multi_strerror(mcode));

	ReadInfo();
}

/**
 * Check for finished HTTP responses.
 *
 * Runs in the I/O thread.  The caller must not hold locks.
 */
inline void
CurlMulti::ReadInfo()
{
	assert(io_thread_inside());

	CURLMsg *msg;
	int msgs_in_queue;

	while ((msg = curl_multi_info_read(multi,
					   &msgs_in_queue)) != nullptr) {
		if (msg->msg == CURLMSG_DONE)
			input_curl_handle_done(msg->easy_handle, msg->data.result);
	}
}

int
CurlMulti::TimerFunction(gcc_unused CURLM *_multi, long timeout_ms, void *userp)
{
	CurlMulti &multi = *(CurlMulti *)userp;
	assert(_multi == multi.multi);

	if (timeout_ms < 0) {
		multi.Cancel();
		return 0;
	}

	if (timeout_ms >= 0 && timeout_ms < 10)
		/* CURL 7.21.1 likes to report "timeout=0", which
		   means we're running in a busy loop.  Quite a bad
		   idea to waste so much CPU.  Let's use a lower limit
		   of 10ms. */
		timeout_ms = 10;

	multi.Schedule(timeout_ms);
	return 0;
}

void
CurlMulti::OnTimeout()
{
	SocketAction(CURL_SOCKET_TIMEOUT, 0);
}

/*
 * InputPlugin methods
 *
 */

static InputPlugin::InitResult
input_curl_init(const config_param &param, Error &error)
{
	CURLcode code = curl_global_init(CURL_GLOBAL_ALL);
	if (code != CURLE_OK) {
		error.Format(curl_domain, code,
			     "curl_global_init() failed: %s",
			     curl_easy_strerror(code));
		return InputPlugin::InitResult::UNAVAILABLE;
	}

	const auto version_info = curl_version_info(CURLVERSION_FIRST);
	if (version_info != nullptr) {
		FormatDebug(curl_domain, "version %s", version_info->version);
		if (version_info->features & CURL_VERSION_SSL)
			FormatDebug(curl_domain, "with %s",
				    version_info->ssl_version);

		curl_version_num = version_info->version_num;
	}

	http_200_aliases = curl_slist_append(http_200_aliases, "ICY 200 OK");

	proxy = param.GetBlockValue("proxy");
	proxy_port = param.GetBlockValue("proxy_port", 0u);
	proxy_user = param.GetBlockValue("proxy_user");
	proxy_password = param.GetBlockValue("proxy_password");

	if (proxy == nullptr) {
		/* deprecated proxy configuration */
		proxy = config_get_string(CONF_HTTP_PROXY_HOST, nullptr);
		proxy_port = config_get_positive(CONF_HTTP_PROXY_PORT, 0);
		proxy_user = config_get_string(CONF_HTTP_PROXY_USER, nullptr);
		proxy_password = config_get_string(CONF_HTTP_PROXY_PASSWORD,
						   "");
	}

	CURLM *multi = curl_multi_init();
	if (multi == nullptr) {
		curl_slist_free_all(http_200_aliases);
		curl_global_cleanup();
		error.Set(curl_domain, 0, "curl_multi_init() failed");
		return InputPlugin::InitResult::UNAVAILABLE;
	}

	curl_multi = new CurlMulti(io_thread_get(), multi);
	return InputPlugin::InitResult::SUCCESS;
}

static void
input_curl_finish(void)
{
	BlockingCall(io_thread_get(), [](){
			delete curl_multi;
		});

	curl_slist_free_all(http_200_aliases);

	curl_global_cleanup();
}

CurlInputStream::~CurlInputStream()
{
	delete tag;

	FreeEasyIndirect();

	buffer.Clear();
	HugeFree(buffer.Write().data, CURL_MAX_BUFFERED);
}

inline bool
CurlInputStream::Check(Error &error)
{
	bool success = !postponed_error.IsDefined();
	if (!success) {
		error = std::move(postponed_error);
		postponed_error.Clear();
	}

	return success;
}

Tag *
CurlInputStream::ReadTag()
{
	Tag *result = tag;
	tag = nullptr;
	return result;
}

inline bool
CurlInputStream::FillBuffer(Error &error)
{
	while (easy != nullptr && buffer.IsEmpty())
		cond.wait(mutex);

	if (postponed_error.IsDefined()) {
		error = std::move(postponed_error);
		postponed_error.Clear();
		return false;
	}

	return !buffer.IsEmpty();
}

size_t
CurlInputStream::Read(void *ptr, size_t read_size, Error &error)
{
	if (!FillBuffer(error))
		return 0;

	auto r = buffer.Read();
	if (r.IsEmpty())
		return 0;

	const size_t nbytes = std::min(read_size, r.size);
	memcpy(ptr, r.data, nbytes);
	buffer.Consume(nbytes);

	offset += (InputPlugin::offset_type)nbytes;

	if (paused && GetTotalBufferSize() < CURL_RESUME_AT) {
		mutex.unlock();

		BlockingCall(io_thread_get(), [this](){
				Resume();
			});

		mutex.lock();
	}

	return nbytes;
}

inline void
CurlInputStream::HeaderReceived(const char *name, std::string &&value)
{
	if (StringEqualsCaseASCII(name, "accept-ranges")) {
		/* a stream with icy-metadata is not seekable */
		if (!icy->IsEnabled())
			seekable = true;
	} else if (StringEqualsCaseASCII(name, "content-length")) {
		size = offset + ParseUint64(value.c_str());
	} else if (StringEqualsCaseASCII(name, "content-type")) {
		SetMimeType(std::move(value));
	} else if (StringEqualsCaseASCII(name, "icy-name") ||
		   StringEqualsCaseASCII(name, "ice-name") ||
		   StringEqualsCaseASCII(name, "x-audiocast-name")) {
		delete tag;

		TagBuilder tag_builder;
		tag_builder.AddItem(TAG_NAME, value.c_str());

		tag = tag_builder.CommitNew();
	} else if (StringEqualsCaseASCII(name, "icy-metaint")) {
		if (icy->IsEnabled())
			return;

		size_t icy_metaint = ParseUint64(value.c_str());
		FormatDebug(curl_domain, "icy-metaint=%zu", icy_metaint);

		if (icy_metaint > 0) {
			icy->Enable(icy_metaint);

			/* a stream with icy-metadata is not
			   seekable */
			seekable = false;
		}
	}
}

/** called by curl when new data is available */
static size_t
input_curl_headerfunction(void *ptr, size_t size, size_t nmemb, void *stream)
{
	CurlInputStream &c = *(CurlInputStream *)stream;

	size *= nmemb;

	const char *header = (const char *)ptr;
	const char *end = header + size;

	char name[64];

	const char *value = (const char *)memchr(header, ':', size);
	if (value == nullptr || (size_t)(value - header) >= sizeof(name))
		return size;

	memcpy(name, header, value - header);
	name[value - header] = 0;

	/* skip the colon */

	++value;

	/* strip the value */

	while (value < end && IsWhitespaceOrNull(*value))
		++value;

	while (end > value && IsWhitespaceOrNull(end[-1]))
		--end;

	c.HeaderReceived(name, std::string(value, end));
	return size;
}

inline size_t
CurlInputStream::DataReceived(const void *ptr, size_t received_size)
{
	assert(received_size > 0);

	const ScopeLock protect(mutex);

	if (received_size > buffer.GetSpace()) {
		paused = true;
		return CURL_WRITEFUNC_PAUSE;
	}

	auto w = buffer.Write();
	assert(!w.IsEmpty());

	size_t nbytes = std::min(w.size, received_size);
	memcpy(w.data, ptr, nbytes);
	buffer.Append(nbytes);

	const size_t remaining = received_size - nbytes;
	if (remaining > 0) {
		w = buffer.Write();
		assert(!w.IsEmpty());
		assert(w.size >= remaining);

		memcpy(w.data, (const uint8_t *)ptr + nbytes, remaining);
		buffer.Append(remaining);
	}

	ready = true;
	cond.broadcast();
	return received_size;
}

/** called by curl when new data is available */
static size_t
input_curl_writefunction(void *ptr, size_t size, size_t nmemb, void *stream)
{
	CurlInputStream &c = *(CurlInputStream *)stream;

	size *= nmemb;
	if (size == 0)
		return 0;

	return c.DataReceived(ptr, size);
}

bool
CurlInputStream::InitEasy(Error &error)
{
	easy = curl_easy_init();
	if (easy == nullptr) {
		error.Set(curl_domain, "curl_easy_init() failed");
		return false;
	}

	curl_easy_setopt(easy, CURLOPT_PRIVATE, (void *)this);
	curl_easy_setopt(easy, CURLOPT_USERAGENT,
			 "Music Player Daemon " VERSION);
	curl_easy_setopt(easy, CURLOPT_HEADERFUNCTION,
			 input_curl_headerfunction);
	curl_easy_setopt(easy, CURLOPT_WRITEHEADER, this);
	curl_easy_setopt(easy, CURLOPT_WRITEFUNCTION,
			 input_curl_writefunction);
	curl_easy_setopt(easy, CURLOPT_WRITEDATA, this);
	curl_easy_setopt(easy, CURLOPT_HTTP200ALIASES, http_200_aliases);
	curl_easy_setopt(easy, CURLOPT_FOLLOWLOCATION, 1);
	curl_easy_setopt(easy, CURLOPT_NETRC, 1);
	curl_easy_setopt(easy, CURLOPT_MAXREDIRS, 5);
	curl_easy_setopt(easy, CURLOPT_FAILONERROR, true);
	curl_easy_setopt(easy, CURLOPT_ERRORBUFFER, error_buffer);
	curl_easy_setopt(easy, CURLOPT_NOPROGRESS, 1l);
	curl_easy_setopt(easy, CURLOPT_NOSIGNAL, 1l);
	curl_easy_setopt(easy, CURLOPT_CONNECTTIMEOUT, 10l);

	if (proxy != nullptr)
		curl_easy_setopt(easy, CURLOPT_PROXY, proxy);

	if (proxy_port > 0)
		curl_easy_setopt(easy, CURLOPT_PROXYPORT, (long)proxy_port);

	if (proxy_user != nullptr && proxy_password != nullptr) {
		char proxy_auth_str[1024];
		snprintf(proxy_auth_str, sizeof(proxy_auth_str),
			 "%s:%s",
			 proxy_user, proxy_password);
		curl_easy_setopt(easy, CURLOPT_PROXYUSERPWD, proxy_auth_str);
	}

	CURLcode code = curl_easy_setopt(easy, CURLOPT_URL, GetURI());
	if (code != CURLE_OK) {
		error.Format(curl_domain, code,
			     "curl_easy_setopt() failed: %s",
			     curl_easy_strerror(code));
		return false;
	}

	request_headers = nullptr;
	request_headers = curl_slist_append(request_headers,
					       "Icy-Metadata: 1");
	curl_easy_setopt(easy, CURLOPT_HTTPHEADER, request_headers);

	return true;
}

inline bool
CurlInputStream::Seek(InputPlugin::offset_type new_offset, int whence,
		      Error &error)
{
	assert(IsReady());

	if (whence == SEEK_SET && new_offset == offset)
		/* no-op */
		return true;

	if (!IsSeekable())
		return false;

	/* calculate the absolute offset */

	switch (whence) {
	case SEEK_SET:
		break;

	case SEEK_CUR:
		new_offset += offset;
		break;

	case SEEK_END:
		if (size < 0)
			/* stream size is not known */
			return false;

		new_offset += size;
		break;

	default:
		return false;
	}

	if (new_offset < 0)
		return false;

	/* check if we can fast-forward the buffer */

	while (new_offset > offset) {
		auto r = buffer.Read();
		if (r.IsEmpty())
			break;

		const size_t nbytes =
			new_offset - offset < (InputPlugin::offset_type)r.size
						   ? new_offset - offset
						   : r.size;

		buffer.Consume(nbytes);
		offset += nbytes;
	}

	if (new_offset == offset)
		return true;

	/* close the old connection and open a new one */

	mutex.unlock();

	FreeEasyIndirect();
	buffer.Clear();

	offset = new_offset;
	if (offset == size) {
		/* seek to EOF: simulate empty result; avoid
		   triggering a "416 Requested Range Not Satisfiable"
		   response */
		mutex.lock();
		return true;
	}

	if (!InitEasy(error)) {
		mutex.lock();
		return false;
	}

	/* send the "Range" header */

	if (offset > 0) {
		sprintf(range, "%lld-", (long long)offset);
		curl_easy_setopt(easy, CURLOPT_RANGE, range);
	}

	ready = false;

	if (!input_curl_easy_add_indirect(this, error)) {
		mutex.lock();
		return false;
	}

	mutex.lock();
	WaitReady();

	if (postponed_error.IsDefined()) {
		error = std::move(postponed_error);
		postponed_error.Clear();
		return false;
	}

	return true;
}

inline InputStream *
CurlInputStream::Open(const char *url, Mutex &mutex, Cond &cond,
		      Error &error)
{
	void *buffer = HugeAllocate(CURL_MAX_BUFFERED);
	if (buffer == nullptr) {
		error.Set(curl_domain, "Out of memory");
		return nullptr;
	}

	CurlInputStream *c = new CurlInputStream(url, mutex, cond, buffer);

	if (!c->InitEasy(error) || !input_curl_easy_add_indirect(c, error)) {
		delete c;
		return nullptr;
	}

	return c->icy;
}

static InputStream *
input_curl_open(const char *url, Mutex &mutex, Cond &cond,
		Error &error)
{
	if (memcmp(url, "http://",  7) != 0 &&
	    memcmp(url, "https://", 8) != 0)
		return nullptr;

	return CurlInputStream::Open(url, mutex, cond, error);
}

const struct InputPlugin input_plugin_curl = {
	"curl",
	input_curl_init,
	input_curl_finish,
	input_curl_open,
};
