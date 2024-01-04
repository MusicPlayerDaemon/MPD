// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "CurlInputPlugin.hxx"
#include "lib/curl/HttpStatusError.hxx"
#include "lib/curl/Global.hxx"
#include "lib/curl/Init.hxx"
#include "lib/curl/Request.hxx"
#include "lib/curl/Handler.hxx"
#include "lib/curl/Slist.hxx"
#include "../MaybeBufferedInputStream.hxx"
#include "../AsyncInputStream.hxx"
#include "../IcyInputStream.hxx"
#include "tag/IcyMetaDataParser.hxx"
#include "../InputPlugin.hxx"
#include "config/Block.hxx"
#include "tag/Builder.hxx"
#include "tag/Tag.hxx"
#include "lib/fmt/ToBuffer.hxx"
#include "event/Call.hxx"
#include "event/Loop.hxx"
#include "util/ASCII.hxx"
#include "util/NumberParser.hxx"
#include "util/Domain.hxx"
#include "Log.hxx"
#include "PluginUnavailable.hxx"
#include "config.h"

#ifdef HAVE_ICU_CONVERTER
#include "lib/icu/Converter.hxx"
#include "util/AllocatedString.hxx"
#include "util/UriExtract.hxx"
#include "util/UriQueryParser.hxx"
#endif

#include <fmt/format.h>

#include <cassert>
#include <cinttypes>

#include <string.h>

#include <curl/curl.h>

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

class CurlInputStream final : public AsyncInputStream, CurlResponseHandler {
	/* some buffers which were passed to libcurl, which we have
	   too free */
	CurlSlist request_headers;

	CurlRequest *request = nullptr;

	/** parser for icy-metadata */
	std::shared_ptr<IcyMetaDataParser> icy;

public:
	template<typename I>
	CurlInputStream(EventLoop &event_loop, const char *_url,
			const Curl::Headers &headers,
			I &&_icy,
			Mutex &_mutex);

	~CurlInputStream() noexcept override;

	CurlInputStream(const CurlInputStream &) = delete;
	CurlInputStream &operator=(const CurlInputStream &) = delete;

	static InputStreamPtr Open(const char *url,
				   const Curl::Headers &headers,
				   Mutex &mutex);

private:
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
	void FreeEasy() noexcept;

	/**
	 * Frees the current "libcurl easy" handle, and everything associated
	 * with it.
	 *
	 * The mutex must not be locked.
	 */
	void FreeEasyIndirect() noexcept;

	/**
	 * The DoSeek() implementation invoked in the IOThread.
	 */
	void SeekInternal(offset_type new_offset);

	/* virtual methods from CurlResponseHandler */
	void OnHeaders(unsigned status, Curl::Headers &&headers) override;
	void OnData(std::span<const std::byte> data) override;
	void OnEnd() override;
	void OnError(std::exception_ptr e) noexcept override;

	/* virtual methods from AsyncInputStream */
	void DoResume() override;
	void DoSeek(offset_type new_offset) override;
};

/** libcurl should accept "ICY 200 OK" */
static struct curl_slist *http_200_aliases;

/** HTTP proxy settings */
static const char *proxy, *proxy_user, *proxy_password;
static unsigned proxy_port;
/** CA CERT settings*/
static const char *cacert;

static bool verify_peer, verify_host;

/** Connection settings */
static long connect_timeout;

/**
 * CURLOPT_VERBOSE - verbose mode
 * DEFAULT 0, meaning disabled.
 */
static bool verbose = false;

/**
 * CURLOPT_LOW_SPEED_LIMIT - low speed limit in bytes per second
 * DEFAULT 0, disabled
 */
static const unsigned default_low_speed_limit = 0;
static long low_speed_limit = default_low_speed_limit;

/**
 * CURLOPT_LOW_SPEED_TIME - low speed limit time period in seconds
 * DEFAULT 0, disabled
 */
static const unsigned default_low_speed_time = 0;
static long low_speed_time = default_low_speed_time;

/**
 * CURLOPT_TCP_KEEPALIVE - TCP keep-alive probing
 * DEFAULT false, disabled
 */
static const bool default_tcp_keepalive = false;
static bool tcp_keepalive = default_tcp_keepalive;

/**
 * CURLOPT_TCP_KEEPIDLE - TCP keep-alive idle time wait in seconds
 * DEFAULT 60 seconds
 */
static const unsigned default_tcp_keepidle = 60;
static long tcp_keepidle = default_tcp_keepidle;

/**
 * CURLOPT_TCP_KEEPINTVL - TCP keep-alive interval in seconds
 * DEFAULT 60 seconds
 */
static const unsigned default_tcp_keepintvl = 60;
static long tcp_keepintvl = default_tcp_keepintvl;


static CurlInit *curl_init;

static constexpr Domain curl_domain("curl");

void
CurlInputStream::DoResume()
{
	assert(GetEventLoop().IsInside());

	const ScopeUnlock unlock(mutex);
	request->Resume();
}

void
CurlInputStream::FreeEasy() noexcept
{
	assert(GetEventLoop().IsInside());

	if (request == nullptr)
		return;

	delete request;
	request = nullptr;
}

void
CurlInputStream::FreeEasyIndirect() noexcept
{
	BlockingCall(GetEventLoop(), [this](){
			FreeEasy();
		});
}

#ifdef HAVE_ICU_CONVERTER

static std::unique_ptr<IcuConverter>
CreateIcuConverterForUri(const char *uri)
{
	const char *fragment = uri_get_fragment(uri);
	if (fragment == nullptr)
		return nullptr;

	const auto charset = UriFindRawQueryParameter(fragment, "charset");
	if (charset.data() == nullptr)
		return nullptr;

	return IcuConverter::Create(std::string{charset}.c_str());
}

#endif

template<typename F>
static void
WithConvertedTagValue(const char *uri, const char *value, F &&f) noexcept
{
#ifdef HAVE_ICU_CONVERTER
	try {
		auto converter = CreateIcuConverterForUri(uri);
		if (converter) {
			f(converter->ToUTF8(value).c_str());
			return;
		}
	} catch (...) {
	}
#else
	(void)uri;
#endif

	f(value);
}

void
CurlInputStream::OnHeaders(unsigned status,
			   Curl::Headers &&headers)
{
	assert(GetEventLoop().IsInside());
	assert(!postponed_exception);
	assert(!icy || !icy->IsDefined());

	if (status < 200 || status >= 300)
		throw HttpStatusError(status,
				      FmtBuffer<40>("got HTTP status {}",
						    status).c_str());

	const std::scoped_lock<Mutex> protect(mutex);

	if (IsSeekPending()) {
		/* don't update metadata while seeking */
		SeekDone();
		return;
	}

	if (headers.find("accept-ranges") != headers.end())
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

		WithConvertedTagValue(GetURI(), i->second.c_str(),
				      [&tag_builder](const char *value){
					      tag_builder.AddItem(TAG_NAME,
								  value);
				      });

		SetTag(tag_builder.CommitNew());
	}

	if (icy) {
		i = headers.find("icy-metaint");

		if (i != headers.end()) {
			size_t icy_metaint = ParseUint64(i->second.c_str());
			FmtDebug(curl_domain, "icy-metaint={}", icy_metaint);

			if (icy_metaint > 0) {
				icy->Start(icy_metaint);

				/* a stream with icy-metadata is not
				   seekable */
				seekable = false;
			}
		}
	}

	SetReady();
}

void
CurlInputStream::OnData(std::span<const std::byte> data)
{
	assert(!data.empty());

	const std::scoped_lock<Mutex> protect(mutex);

	if (IsSeekPending())
		SeekDone();

	if (data.size() > GetBufferSpace()) {
		AsyncInputStream::Pause();
		throw CurlResponseHandler::Pause{};
	}

	AppendToBuffer(data);
}

void
CurlInputStream::OnEnd()
{
	const std::scoped_lock<Mutex> protect(mutex);
	InvokeOnAvailable();

	AsyncInputStream::SetClosed();
}

void
CurlInputStream::OnError(std::exception_ptr e) noexcept
{
	const std::scoped_lock<Mutex> protect(mutex);
	postponed_exception = std::move(e);

	if (IsSeekPending())
		SeekDone();
	else if (!IsReady())
		SetReady();
	else
		InvokeOnAvailable();

	AsyncInputStream::SetClosed();
}

/*
 * InputPlugin methods
 *
 */

static void
input_curl_init(EventLoop &event_loop, const ConfigBlock &block)
{
	try {
		curl_init = new CurlInit(event_loop);
	} catch (...) {
		std::throw_with_nested(PluginUnavailable("CURL initialization failed"));
	}

	const auto version_info = curl_version_info(CURLVERSION_FIRST);
	if (version_info != nullptr) {
		FmtDebug(curl_domain, "version {}", version_info->version);
		if (version_info->features & CURL_VERSION_SSL)
			FmtDebug(curl_domain, "with {}",
				 version_info->ssl_version);
	}

	http_200_aliases = curl_slist_append(http_200_aliases, "ICY 200 OK");

	proxy = block.GetBlockValue("proxy");
	proxy_port = block.GetBlockValue("proxy_port", 0U);
	proxy_user = block.GetBlockValue("proxy_user");
	proxy_password = block.GetBlockValue("proxy_password");

#ifdef ANDROID
	// TODO: figure out how to use Android's CA certificates and re-enable verify
	constexpr bool default_verify = false;
#else
	constexpr bool default_verify = true;
#endif
	cacert = block.GetBlockValue("cacert");
	verify_peer = block.GetBlockValue("verify_peer", default_verify);
	verify_host = block.GetBlockValue("verify_host", default_verify);

	constexpr unsigned default_connection_timeout = 10;
	unsigned timeout = block.GetBlockValue("connect_timeout",
					       default_connection_timeout);
	connect_timeout = static_cast<long>(timeout);

	verbose = block.GetBlockValue("verbose",verbose);

	low_speed_limit = block.GetBlockValue("low_speed_limit", default_low_speed_limit);

	low_speed_time = block.GetBlockValue("low_speed_time", default_low_speed_time);

	tcp_keepalive = block.GetBlockValue("tcp_keepalive",default_tcp_keepalive);

	tcp_keepidle  = block.GetBlockValue("tcp_keepidle",default_tcp_keepidle);

	tcp_keepintvl = block.GetBlockValue("tcp_keepintvl",default_tcp_keepintvl);
}

static void
input_curl_finish() noexcept
{
	delete curl_init;

	curl_slist_free_all(http_200_aliases);
	http_200_aliases = nullptr;
}

template<typename I>
inline
CurlInputStream::CurlInputStream(EventLoop &event_loop, const char *_url,
				 const Curl::Headers &headers,
				 I &&_icy,
				 Mutex &_mutex)
	:AsyncInputStream(event_loop, _url, _mutex,
			  CURL_MAX_BUFFERED,
			  CURL_RESUME_AT),
	 icy(std::forward<I>(_icy))
{
	request_headers.Append("Icy-Metadata: 1");

	for (const auto &[key, header] : headers)
		request_headers.Append((key + ":" += header).c_str());
}

CurlInputStream::~CurlInputStream() noexcept
{
	FreeEasyIndirect();
}

static CurlEasy
CreateEasy(const char *url, struct curl_slist *headers)
{
	CurlEasy easy{url};

	/* increase CURL's receive buffer size from 16 kB to 512 kB
	   (the maximum until CURL 7.88.0) to reduce system call
	   overhead */
	easy.TrySetOption(CURLOPT_BUFFERSIZE, 512L * 1024L);

	easy.SetOption(CURLOPT_HTTP200ALIASES, http_200_aliases);
	easy.SetOption(CURLOPT_FOLLOWLOCATION, 1L);
	easy.SetOption(CURLOPT_MAXREDIRS, 5L);

	/* this option eliminates the probe request when
	   username/password are specified */
	easy.SetOption(CURLOPT_HTTPAUTH, CURLAUTH_BASIC);

	if (proxy != nullptr)
		easy.SetOption(CURLOPT_PROXY, proxy);

	if (proxy_port > 0)
		easy.SetOption(CURLOPT_PROXYPORT, (long)proxy_port);

	if (proxy_user != nullptr && proxy_password != nullptr)
		easy.SetOption(CURLOPT_PROXYUSERPWD,
			       FmtBuffer<1024>("{}:{}", proxy_user,
					       proxy_password).c_str());

	if (cacert != nullptr)
		easy.SetOption(CURLOPT_CAINFO, cacert);
	easy.SetVerifyPeer(verify_peer);
	easy.SetVerifyHost(verify_host);

	try {
		easy.SetProxyVerifyPeer(verify_peer);
		easy.SetProxyVerifyHost(verify_host);
	} catch (...) {
		/* these methods fail if libCURL was compiled with
		   CURL_DISABLE_PROXY; ignore silently */
	}

	easy.SetConnectTimeout(connect_timeout);

	easy.SetOption(CURLOPT_VERBOSE, verbose ? 1 : 0);

	easy.SetOption(CURLOPT_LOW_SPEED_LIMIT, low_speed_limit);
	easy.SetOption(CURLOPT_LOW_SPEED_TIME, low_speed_time);

	easy.SetOption(CURLOPT_TCP_KEEPALIVE, tcp_keepalive ? 1 : 0);
	easy.SetOption(CURLOPT_TCP_KEEPIDLE, tcp_keepidle);
	easy.SetOption(CURLOPT_TCP_KEEPINTVL, tcp_keepintvl);

	easy.SetRequestHeaders(headers);

	return easy;
}

void
CurlInputStream::InitEasy()
{
	request = new CurlRequest(**curl_init,
				  CreateEasy(GetURI(), request_headers.Get()),
				  *this);
}

void
CurlInputStream::StartRequest()
{
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

	if (offset > 0)
		request->GetEasy().SetOption(CURLOPT_RANGE,
					     fmt::format_int{offset}.c_str());

	StartRequest();
}

void
CurlInputStream::DoSeek(offset_type new_offset)
{
	assert(IsReady());
	assert(seekable);

	const ScopeUnlock unlock(mutex);

	BlockingCall(GetEventLoop(), [this, new_offset](){
			SeekInternal(new_offset);
		});
}

inline InputStreamPtr
CurlInputStream::Open(const char *url,
		      const Curl::Headers &headers,
		      Mutex &mutex)
{
	auto icy = std::make_shared<IcyMetaDataParser>();

	auto c = std::make_unique<CurlInputStream>((*curl_init)->GetEventLoop(),
						   url, headers,
						   icy,
						   mutex);

	BlockingCall(c->GetEventLoop(), [&c](){
			c->InitEasy();
			c->StartRequest();
		});

	return std::make_unique<MaybeBufferedInputStream>(std::make_unique<IcyInputStream>(std::move(c), std::move(icy)));
}

InputStreamPtr
OpenCurlInputStream(const char *uri, const Curl::Headers &headers,
		    Mutex &mutex)
{
	return CurlInputStream::Open(uri, headers, mutex);
}

static InputStreamPtr
input_curl_open(const char *url, Mutex &mutex)
{
	if (!StringStartsWithCaseASCII(url, "http://") &&
	    !StringStartsWithCaseASCII(url, "https://"))
		return nullptr;

	return CurlInputStream::Open(url, {}, mutex);
}

static std::set<std::string, std::less<>>
input_curl_protocols() noexcept
{
	std::set<std::string, std::less<>> protocols;
	auto version_info = curl_version_info(CURLVERSION_FIRST);
	for (auto proto_ptr = version_info->protocols; *proto_ptr != nullptr; proto_ptr++) {
		if (protocol_is_whitelisted(*proto_ptr)) {
			std::string schema(*proto_ptr);
			schema.append("://");
			protocols.emplace(schema);
		}
	}
	return protocols;
}

const struct InputPlugin input_plugin_curl = {
	"curl",
	nullptr,
	input_curl_init,
	input_curl_finish,
	input_curl_open,
	input_curl_protocols
};
