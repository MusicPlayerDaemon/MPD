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
#include "lib/curl/Global.hxx"
#include "lib/curl/Request.hxx"
#include "../AsyncInputStream.hxx"
#include "../IcyInputStream.hxx"
#include "../InputPlugin.hxx"
#include "config/ConfigGlobal.hxx"
#include "config/Block.hxx"
#include "tag/TagBuilder.hxx"
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

struct CurlInputStream final : public AsyncInputStream, CurlRequest {
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
	 * The DoSeek() implementation invoked in the IOThread.
	 */
	void SeekInternal(offset_type new_offset);

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

	/* virtual methods from CurlRequest */
	void Done(CURLcode result) override;

	/* virtual methods from AsyncInputStream */
	virtual void DoResume() override;
	virtual void DoSeek(offset_type new_offset) override;
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
			curl_global->Add(c->easy.Get(), *c);
		});
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

	const std::lock_guard<Mutex> protect(mutex);

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

void
CurlInputStream::Done(CURLcode result)
{
	long status = 0;
	curl_easy_getinfo(easy.Get(), CURLINFO_RESPONSE_CODE, &status);

	RequestDone(result, status);
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

	try {
		curl_global = new CurlGlobal(io_thread_get());
	} catch (const std::runtime_error &e) {
		LogError(e);
		curl_slist_free_all(http_200_aliases);
		curl_global_cleanup();
		throw PluginUnavailable("curl_multi_init() failed");
	}
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

	const std::lock_guard<Mutex> protect(mutex);

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
CurlInputStream::SeekInternal(offset_type new_offset)
{
	/* close the old connection and open a new one */

	FreeEasy();

	offset = new_offset;
	if (offset == size)
		/* seek to EOF: simulate empty result; avoid
		   triggering a "416 Requested Range Not Satisfiable"
		   response */
		return;

	InitEasy();

	/* send the "Range" header */

	if (offset > 0) {
		sprintf(range, "%lld-", (long long)offset);
		easy.SetOption(CURLOPT_RANGE, range);
	}

	curl_global->Add(easy.Get(), *this);
}

void
CurlInputStream::DoSeek(offset_type new_offset)
{
	assert(IsReady());

	const ScopeUnlock unlock(mutex);

	BlockingCall(io_thread_get(), [this, new_offset](){
			SeekInternal(new_offset);
		});
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
