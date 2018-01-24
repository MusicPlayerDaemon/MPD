/*
 * Copyright 2003-2017 The Music Player Daemon Project
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
#include "lib/curl/Handler.hxx"
#include "lib/curl/Slist.hxx"
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

struct CurlInputStream final : public AsyncInputStream, CurlResponseHandler {
	/* some buffers which were passed to libcurl, which we have
	   too free */
	CurlSlist request_headers;

	CurlRequest *request = nullptr;

	/** parser for icy-metadata */
	IcyInputStream *icy;

	CurlInputStream(const char *_url, Mutex &_mutex, Cond &_cond)
		:AsyncInputStream(_url, _mutex, _cond,
				  CURL_MAX_BUFFERED,
				  CURL_RESUME_AT),
		 icy(new IcyInputStream(this)) {
	}

	~CurlInputStream();

	CurlInputStream(const CurlInputStream &) = delete;
	CurlInputStream &operator=(const CurlInputStream &) = delete;

	static InputStream *Open(const char *url, Mutex &mutex, Cond &cond);

	/**
	 * Create and initialize a new #CurlRequest instance.  After
	 * this, you may add more request headers and set options.  To
	 * actually start the request, call StartRequest().
	 */
	void InitEasy();

	/**
	 * Start the request after having called InitEasy().  After
	 * this, you must not set any CURL options.
	 */
	void StartRequest();

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

	/* virtual methods from CurlResponseHandler */
	void OnHeaders(unsigned status,
		       std::multimap<std::string, std::string> &&headers) override;
	void OnData(ConstBuffer<void> data) override;
	void OnEnd() override;
	void OnError(std::exception_ptr e) override;

	/* virtual methods from AsyncInputStream */
	virtual void DoResume() override;
	virtual void DoSeek(offset_type new_offset) override;
};

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
	request->Resume();
	mutex.lock();
}

void
CurlInputStream::FreeEasy()
{
	assert(io_thread_inside());

	if (request == nullptr)
		return;

	delete request;
	request = nullptr;

	request_headers.Clear();
}

void
CurlInputStream::FreeEasyIndirect()
{
	BlockingCall(io_thread_get(), [this](){
			FreeEasy();
			curl_global->InvalidateSockets();
		});
}

void
CurlInputStream::OnHeaders(unsigned status,
			   std::multimap<std::string, std::string> &&headers)
{
	assert(io_thread_inside());
	assert(!postponed_exception);

	if (status < 200 || status >= 300)
		throw FormatRuntimeError("got HTTP status %ld", status);

	const std::lock_guard<Mutex> protect(mutex);

	if (IsSeekPending()) {
		/* don't update metadata while seeking */
		SeekDone();
		return;
	}

	if (!icy->IsEnabled() &&
	    headers.find("accept-ranges") != headers.end())
		/* a stream with icy-metadata is not seekable */
		seekable = true;

	auto i = headers.find("content-length");
	if (i != headers.end())
		size = offset + ParseUint64(i->second.c_str());

	i = headers.find("content-type");
	if (i != headers.end())
		SetMimeType(std::move(i->second));

	i = headers.find("icy-name");
	if (i == headers.end()) {
		i = headers.find("ice-name");
		if (i == headers.end())
			i = headers.find("x-audiocast-name");
	}

	if (i != headers.end()) {
		TagBuilder tag_builder;
		tag_builder.AddItem(TAG_NAME, i->second.c_str());

		SetTag(tag_builder.CommitNew());
	}

	if (!icy->IsEnabled()) {
		i = headers.find("icy-metaint");

		if (i != headers.end()) {
			size_t icy_metaint = ParseUint64(i->second.c_str());
#ifndef _WIN32
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

	SetReady();
}

void
CurlInputStream::OnData(ConstBuffer<void> data)
{
	assert(data.size > 0);

	const std::lock_guard<Mutex> protect(mutex);

	if (IsSeekPending())
		SeekDone();

	if (data.size > GetBufferSpace()) {
		AsyncInputStream::Pause();
		throw CurlRequest::Pause();
	}

	AppendToBuffer(data.data, data.size);
}

void
CurlInputStream::OnEnd()
{
	const std::lock_guard<Mutex> protect(mutex);
	cond.broadcast();

	AsyncInputStream::SetClosed();
}

void
CurlInputStream::OnError(std::exception_ptr e)
{
	const std::lock_guard<Mutex> protect(mutex);
	postponed_exception = std::move(e);

	if (IsSeekPending())
		SeekDone();
	else if (!IsReady())
		SetReady();
	else
		cond.broadcast();

	AsyncInputStream::SetClosed();
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

void
CurlInputStream::InitEasy()
{
	request = new CurlRequest(*curl_global, GetURI(), *this);

	request->SetOption(CURLOPT_HTTP200ALIASES, http_200_aliases);
	request->SetOption(CURLOPT_FOLLOWLOCATION, 1l);
	request->SetOption(CURLOPT_MAXREDIRS, 5l);
	request->SetOption(CURLOPT_FAILONERROR, 1l);

	if (proxy != nullptr)
		request->SetOption(CURLOPT_PROXY, proxy);

	if (proxy_port > 0)
		request->SetOption(CURLOPT_PROXYPORT, (long)proxy_port);

	if (proxy_user != nullptr && proxy_password != nullptr) {
		char proxy_auth_str[1024];
		snprintf(proxy_auth_str, sizeof(proxy_auth_str),
			 "%s:%s",
			 proxy_user, proxy_password);
		request->SetOption(CURLOPT_PROXYUSERPWD, proxy_auth_str);
	}

	request->SetOption(CURLOPT_SSL_VERIFYPEER, verify_peer ? 1l : 0l);
	request->SetOption(CURLOPT_SSL_VERIFYHOST, verify_host ? 2l : 0l);

	request_headers.Clear();
	request_headers.Append("Icy-Metadata: 1");
}

void
CurlInputStream::StartRequest()
{
	request->SetOption(CURLOPT_HTTPHEADER, request_headers.Get());

	request->Start();
}

void
CurlInputStream::SeekInternal(offset_type new_offset)
{
	/* close the old connection and open a new one */

	FreeEasy();

	offset = new_offset;
	if (offset == size) {
		/* seek to EOF: simulate empty result; avoid
		   triggering a "416 Requested Range Not Satisfiable"
		   response */
		SeekDone();
		return;
	}

	InitEasy();

	/* send the "Range" header */

	if (offset > 0) {
		char range[32];
		sprintf(range, "%llu-", (unsigned long long)offset);
		request->SetOption(CURLOPT_RANGE, range);
	}

	StartRequest();
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
		BlockingCall(io_thread_get(), [c](){
				c->InitEasy();
				c->StartRequest();
			});
	} catch (...) {
		delete c;
		throw;
	}

	return c->icy;
}

static InputStream *
input_curl_open(const char *url, Mutex &mutex, Cond &cond)
{
	if (strncmp(url, "http://", 7) != 0 &&
	    strncmp(url, "https://", 8) != 0)
		return nullptr;

	return CurlInputStream::Open(url, mutex, cond);
}

const struct InputPlugin input_plugin_curl = {
	"curl",
	input_curl_init,
	input_curl_finish,
	input_curl_open,
};
