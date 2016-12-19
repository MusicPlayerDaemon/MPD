/*
 * Copyright 2003-2016 The Music Player Daemon Project
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
#include "lib/curl/Easy.hxx"
#include "../AsyncInputStream.hxx"
#include "../IcyInputStream.hxx"
#include "../InputPlugin.hxx"
#include "config/ConfigGlobal.hxx"
#include "config/Block.hxx"
#include "tag/TagBuilder.hxx"
#include "event/SocketMonitor.hxx"
#include "event/TimeoutMonitor.hxx"
#include "event/Call.hxx"
#include "IOThread.hxx"
#include "util/ASCII.hxx"
#include "util/StringUtil.hxx"
#include "util/NumberParser.hxx"
#include "util/RuntimeError.hxx"
#include "util/Domain.hxx"
#include "Log.hxx"
#include "PluginUnavailable.hxx"

#include <assert.h>
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

struct CurlInputStream final : public AsyncInputStream {
	/* some buffers which were passed to libcurl, which we have
	   too free */
	char range[32];
	struct curl_slist *request_headers;

	/** the curl handles */
	CurlEasy easy;

	/** error message provided by libcurl */
	char error_buffer[CURL_ERROR_SIZE];

	/** parser for icy-metadata */
	IcyInputStream *icy;

	CurlInputStream(const char *_url, Mutex &_mutex, Cond &_cond)
		:AsyncInputStream(_url, _mutex, _cond,
				  CURL_MAX_BUFFERED,
				  CURL_RESUME_AT),
		 request_headers(nullptr),
		 easy(nullptr),
		 icy(new IcyInputStream(this)) {
	}

	~CurlInputStream();

	CurlInputStream(const CurlInputStream &) = delete;
	CurlInputStream &operator=(const CurlInputStream &) = delete;

	static InputStream *Open(const char *url, Mutex &mutex, Cond &cond);

	void InitEasy();

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

	/**
	 * Called when a new response begins.  This is used to discard
	 * headers from previous responses (for example authentication
	 * and redirects).
	 */
	void ResponseBoundary();

	void HeaderReceived(const char *name, std::string &&value);

	size_t DataReceived(const void *ptr, size_t size);

	/**
	 * A HTTP request is finished.
	 *
	 * Runs in the I/O thread.  The caller must not hold locks.
	 */
	void RequestDone(CURLcode result, long status);

	/* virtual methods from AsyncInputStream */
	virtual void DoResume() override;
	virtual void DoSeek(offset_type new_offset) override;
};

class CurlGlobal;

/**
 * Monitor for one socket created by CURL.
 */
class CurlSocket final : SocketMonitor {
	CurlGlobal &global;

public:
	CurlSocket(CurlGlobal &_global, EventLoop &_loop, int _fd)
		:SocketMonitor(_fd, _loop), global(_global) {}

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
class CurlGlobal final : private TimeoutMonitor {
	CURLM *const multi;

public:
	CurlGlobal(EventLoop &_loop, CURLM *_multi);

	~CurlGlobal() {
		curl_multi_cleanup(multi);
	}

	void Add(CurlInputStream *c);
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
	static int TimerFunction(CURLM *global, long timeout_ms, void *userp);

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

static bool verify_peer, verify_host;

static CurlGlobal *curl_global;

static constexpr Domain curl_domain("curl");
static constexpr Domain curlm_domain("curlm");

CurlGlobal::CurlGlobal(EventLoop &_loop, CURLM *_mutex)
	:TimeoutMonitor(_loop), multi(_mutex)
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

void
CurlInputStream::DoResume()
{
	assert(io_thread_inside());

	mutex.unlock();

	curl_easy_pause(easy.Get(), CURLPAUSE_CONT);

	if (curl_version_num < 0x072000)
		/* libcurl older than 7.32.0 does not update
		   its sockets after curl_easy_pause(); force
		   libcurl to do it now */
		curl_global->ResumeSockets();

	curl_global->InvalidateSockets();

	mutex.lock();
}

int
CurlSocket::SocketFunction(gcc_unused CURL *easy,
			   curl_socket_t s, int action,
			   void *userp, void *socketp) {
	auto &global = *(CurlGlobal *)userp;
	CurlSocket *cs = (CurlSocket *)socketp;

	assert(io_thread_inside());

	if (action == CURL_POLL_REMOVE) {
		delete cs;
		return 0;
	}

	if (cs == nullptr) {
		cs = new CurlSocket(global, io_thread_get(), s);
		global.Assign(s, *cs);
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

	global.SocketAction(Get(), FlagsToCurlCSelect(flags));
	return true;
}

/**
 * Runs in the I/O thread.  No lock needed.
 *
 * Throws std::runtime_error on error.
 */
inline void
CurlGlobal::Add(CurlInputStream *c)
{
	assert(io_thread_inside());
	assert(c != nullptr);
	assert(c->easy);

	CURLMcode mcode = curl_multi_add_handle(multi, c->easy.Get());
	if (mcode != CURLM_OK)
		throw FormatRuntimeError("curl_multi_add_handle() failed: %s",
					 curl_multi_strerror(mcode));

	InvalidateSockets();
}

/**
 * Call input_curl_easy_add() in the I/O thread.  May be called from
 * any thread.  Caller must not hold a mutex.
 *
 * Throws std::runtime_error on error.
 */
static void
input_curl_easy_add_indirect(CurlInputStream *c)
{
	assert(c != nullptr);
	assert(c->easy);

	BlockingCall(io_thread_get(), [c](){
			curl_global->Add(c);
		});
}

inline void
CurlGlobal::Remove(CurlInputStream *c)
{
	curl_multi_remove_handle(multi, c->easy.Get());
}

void
CurlInputStream::FreeEasy()
{
	assert(io_thread_inside());

	if (!easy)
		return;

	curl_global->Remove(this);

	easy = nullptr;

	curl_slist_free_all(request_headers);
	request_headers = nullptr;
}

void
CurlInputStream::FreeEasyIndirect()
{
	BlockingCall(io_thread_get(), [this](){
			FreeEasy();
			curl_global->InvalidateSockets();
		});

	assert(!easy);
}

inline void
CurlInputStream::RequestDone(CURLcode result, long status)
{
	assert(io_thread_inside());
	assert(!postponed_exception);

	FreeEasy();
	AsyncInputStream::SetClosed();

	const ScopeLock protect(mutex);

	if (result != CURLE_OK) {
		postponed_exception = std::make_exception_ptr(FormatRuntimeError("curl failed: %s",
										 error_buffer));
	} else if (status < 200 || status >= 300) {
		postponed_exception = std::make_exception_ptr(FormatRuntimeError("got HTTP status %ld",
										 status));
	}

	if (IsSeekPending())
		SeekDone();
	else if (!IsReady())
		SetReady();
	else
		cond.broadcast();
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
CurlGlobal::SocketAction(curl_socket_t fd, int ev_bitmask)
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
CurlGlobal::ReadInfo()
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
CurlGlobal::TimerFunction(gcc_unused CURLM *_global, long timeout_ms, void *userp)
{
	auto &global = *(CurlGlobal *)userp;
	assert(_global == global.multi);

	if (timeout_ms < 0) {
		global.Cancel();
		return 0;
	}

	if (timeout_ms >= 0 && timeout_ms < 10)
		/* CURL 7.21.1 likes to report "timeout=0", which
		   means we're running in a busy loop.  Quite a bad
		   idea to waste so much CPU.  Let's use a lower limit
		   of 10ms. */
		timeout_ms = 10;

	global.Schedule(timeout_ms);
	return 0;
}

void
CurlGlobal::OnTimeout()
{
	SocketAction(CURL_SOCKET_TIMEOUT, 0);
}

/*
 * InputPlugin methods
 *
 */

static void
input_curl_init(const ConfigBlock &block)
{
	CURLcode code = curl_global_init(CURL_GLOBAL_ALL);
	if (code != CURLE_OK)
		throw PluginUnavailable(curl_easy_strerror(code));

	const auto version_info = curl_version_info(CURLVERSION_FIRST);
	if (version_info != nullptr) {
		FormatDebug(curl_domain, "version %s", version_info->version);
		if (version_info->features & CURL_VERSION_SSL)
			FormatDebug(curl_domain, "with %s",
				    version_info->ssl_version);

		curl_version_num = version_info->version_num;
	}

	http_200_aliases = curl_slist_append(http_200_aliases, "ICY 200 OK");

	proxy = block.GetBlockValue("proxy");
	proxy_port = block.GetBlockValue("proxy_port", 0u);
	proxy_user = block.GetBlockValue("proxy_user");
	proxy_password = block.GetBlockValue("proxy_password");

	if (proxy == nullptr) {
		/* deprecated proxy configuration */
		proxy = config_get_string(ConfigOption::HTTP_PROXY_HOST);
		proxy_port = config_get_positive(ConfigOption::HTTP_PROXY_PORT, 0);
		proxy_user = config_get_string(ConfigOption::HTTP_PROXY_USER);
		proxy_password = config_get_string(ConfigOption::HTTP_PROXY_PASSWORD,
						   "");
	}

	verify_peer = block.GetBlockValue("verify_peer", true);
	verify_host = block.GetBlockValue("verify_host", true);

	CURLM *global = curl_multi_init();
	if (global == nullptr) {
		curl_slist_free_all(http_200_aliases);
		curl_global_cleanup();
		throw PluginUnavailable("curl_multi_init() failed");
	}

	curl_global = new CurlGlobal(io_thread_get(), global);
}

static void
input_curl_finish(void)
{
	BlockingCall(io_thread_get(), [](){
			delete curl_global;
		});

	curl_slist_free_all(http_200_aliases);
	http_200_aliases = nullptr;

	curl_global_cleanup();
}

CurlInputStream::~CurlInputStream()
{
	FreeEasyIndirect();
}

inline void
CurlInputStream::ResponseBoundary()
{
	/* undo all effects of HeaderReceived() because the previous
	   response was not applicable for this stream */

	if (IsSeekPending())
		/* don't update metadata while seeking */
		return;

	seekable = false;
	size = UNKNOWN_SIZE;
	ClearMimeType();
	ClearTag();

	// TODO: reset the IcyInputStream?
}

inline void
CurlInputStream::HeaderReceived(const char *name, std::string &&value)
{
	if (IsSeekPending())
		/* don't update metadata while seeking */
		return;

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
		TagBuilder tag_builder;
		tag_builder.AddItem(TAG_NAME, value.c_str());

		SetTag(tag_builder.CommitNew());
	} else if (StringEqualsCaseASCII(name, "icy-metaint")) {
		if (icy->IsEnabled())
			return;

		size_t icy_metaint = ParseUint64(value.c_str());
#ifndef WIN32
		/* Windows doesn't know "%z" */
		FormatDebug(curl_domain, "icy-metaint=%zu", icy_metaint);
#endif

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
	if (size > 5 && memcmp(header, "HTTP/", 5) == 0) {
		c.ResponseBoundary();
		return size;
	}

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

	value = StripLeft(value, end);
	end = StripRight(value, end);

	c.HeaderReceived(name, std::string(value, end));
	return size;
}

inline size_t
CurlInputStream::DataReceived(const void *ptr, size_t received_size)
{
	assert(received_size > 0);

	const ScopeLock protect(mutex);

	if (IsSeekPending())
		SeekDone();

	if (received_size > GetBufferSpace()) {
		AsyncInputStream::Pause();
		return CURL_WRITEFUNC_PAUSE;
	}

	AppendToBuffer(ptr, received_size);
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

void
CurlInputStream::InitEasy()
{
	easy = CurlEasy();

	easy.SetOption(CURLOPT_PRIVATE, (void *)this);
	easy.SetOption(CURLOPT_USERAGENT, "Music Player Daemon " VERSION);
	easy.SetOption(CURLOPT_HEADERFUNCTION, input_curl_headerfunction);
	easy.SetOption(CURLOPT_WRITEHEADER, this);
	easy.SetOption(CURLOPT_WRITEFUNCTION, input_curl_writefunction);
	easy.SetOption(CURLOPT_WRITEDATA, this);
	easy.SetOption(CURLOPT_HTTP200ALIASES, http_200_aliases);
	easy.SetOption(CURLOPT_FOLLOWLOCATION, 1l);
	easy.SetOption(CURLOPT_NETRC, 1l);
	easy.SetOption(CURLOPT_MAXREDIRS, 5l);
	easy.SetOption(CURLOPT_FAILONERROR, 1l);
	easy.SetOption(CURLOPT_ERRORBUFFER, error_buffer);
	easy.SetOption(CURLOPT_NOPROGRESS, 1l);
	easy.SetOption(CURLOPT_NOSIGNAL, 1l);
	easy.SetOption(CURLOPT_CONNECTTIMEOUT, 10l);

	if (proxy != nullptr)
		easy.SetOption(CURLOPT_PROXY, proxy);

	if (proxy_port > 0)
		easy.SetOption(CURLOPT_PROXYPORT, (long)proxy_port);

	if (proxy_user != nullptr && proxy_password != nullptr) {
		char proxy_auth_str[1024];
		snprintf(proxy_auth_str, sizeof(proxy_auth_str),
			 "%s:%s",
			 proxy_user, proxy_password);
		easy.SetOption(CURLOPT_PROXYUSERPWD, proxy_auth_str);
	}

	easy.SetOption(CURLOPT_SSL_VERIFYPEER, verify_peer ? 1l : 0l);
	easy.SetOption(CURLOPT_SSL_VERIFYHOST, verify_host ? 2l : 0l);

	easy.SetOption(CURLOPT_URL, GetURI());

	request_headers = nullptr;
	request_headers = curl_slist_append(request_headers,
					       "Icy-Metadata: 1");
	easy.SetOption(CURLOPT_HTTPHEADER, request_headers);
}

void
CurlInputStream::DoSeek(offset_type new_offset)
{
	assert(IsReady());

	/* close the old connection and open a new one */

	mutex.unlock();

	FreeEasyIndirect();

	offset = new_offset;
	if (offset == size) {
		/* seek to EOF: simulate empty result; avoid
		   triggering a "416 Requested Range Not Satisfiable"
		   response */
		mutex.lock();
		SeekDone();
		return;
	}

	try {
		InitEasy();
	} catch (...) {
		mutex.lock();
		throw;
	}

	/* send the "Range" header */

	if (offset > 0) {
		sprintf(range, "%lld-", (long long)offset);
		easy.SetOption(CURLOPT_RANGE, range);
	}

	try {
		input_curl_easy_add_indirect(this);
	} catch (...) {
		mutex.lock();
		throw;
	}

	mutex.lock();
	offset = new_offset;
}

inline InputStream *
CurlInputStream::Open(const char *url, Mutex &mutex, Cond &cond)
{
	CurlInputStream *c = new CurlInputStream(url, mutex, cond);

	try {
		c->InitEasy();
		input_curl_easy_add_indirect(c);
	} catch (...) {
		delete c;
		throw;
	}

	return c->icy;
}

static InputStream *
input_curl_open(const char *url, Mutex &mutex, Cond &cond)
{
	if (memcmp(url, "http://",  7) != 0 &&
	    memcmp(url, "https://", 8) != 0)
		return nullptr;

	return CurlInputStream::Open(url, mutex, cond);
}

const struct InputPlugin input_plugin_curl = {
	"curl",
	input_curl_init,
	input_curl_finish,
	input_curl_open,
};
