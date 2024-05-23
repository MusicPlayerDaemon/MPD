// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "CurlStorage.hxx"
#include "storage/StoragePlugin.hxx"
#include "storage/StorageInterface.hxx"
#include "storage/FileInfo.hxx"
#include "storage/MemoryDirectoryReader.hxx"
#include "input/InputStream.hxx"
#include "input/RewindInputStream.hxx"
#include "input/plugins/CurlInputPlugin.hxx"
#include "lib/curl/HttpStatusError.hxx"
#include "lib/curl/Init.hxx"
#include "lib/curl/Global.hxx"
#include "lib/curl/Slist.hxx"
#include "lib/curl/String.hxx"
#include "lib/curl/Request.hxx"
#include "lib/curl/Handler.hxx"
#include "lib/curl/Escape.hxx"
#include "lib/expat/ExpatParser.hxx"
#include "lib/fmt/ToBuffer.hxx"
#include "fs/Traits.hxx"
#include "event/InjectEvent.hxx"
#include "thread/Mutex.hxx"
#include "thread/Cond.hxx"
#include "util/ASCII.hxx"
#include "util/NumberParser.hxx"
#include "util/SpanCast.hxx"
#include "util/StringCompare.hxx"
#include "util/StringSplit.hxx"
#include "util/UriExtract.hxx"

#include <cassert>
#include <memory>
#include <string>
#include <utility>

using std::string_view_literals::operator""sv;

class CurlStorage final : public Storage {
	const std::string base;

	CurlInit curl;

public:
	CurlStorage(EventLoop &_loop, const char *_base)
		:base(_base),
		 curl(_loop) {}

	/* virtual methods from class Storage */
	StorageFileInfo GetInfo(std::string_view uri_utf8, bool follow) override;

	std::unique_ptr<StorageDirectoryReader> OpenDirectory(std::string_view uri_utf8) override;

	[[nodiscard]] std::string MapUTF8(std::string_view uri_utf8) const noexcept override;

	[[nodiscard]] std::string_view MapToRelativeUTF8(std::string_view uri_utf8) const noexcept override;

	InputStreamPtr OpenFile(std::string_view uri_utf8, Mutex &mutex) override;
};

std::string
CurlStorage::MapUTF8(std::string_view uri_utf8) const noexcept
{
	if (uri_utf8.empty())
		return base;

	std::string path_esc = CurlEscapeUriPath(uri_utf8);
	return PathTraitsUTF8::Build(base, path_esc);
}

std::string_view
CurlStorage::MapToRelativeUTF8(std::string_view uri_utf8) const noexcept
{
	return PathTraitsUTF8::Relative(base,
					CurlUnescape(uri_utf8));
}

InputStreamPtr
CurlStorage::OpenFile(std::string_view uri_utf8, Mutex &mutex)
{
	return input_rewind_open(OpenCurlInputStream(MapUTF8(uri_utf8), {}, mutex));
}

class BlockingHttpRequest : protected CurlResponseHandler {
	InjectEvent defer_start;

	std::exception_ptr postponed_error;

	bool done = false;

protected:
	CurlRequest request;

	Mutex mutex;
	Cond cond;

public:
	BlockingHttpRequest(CurlGlobal &curl, const char *uri)
		:defer_start(curl.GetEventLoop(),
			     BIND_THIS_METHOD(OnDeferredStart)),
		 request(curl, uri, *this) {
		// TODO: use CurlInputStream's configuration
	}

	void DeferStart() noexcept {
		/* start the transfer inside the IOThread */
		defer_start.Schedule();
	}

	void Wait() {
		std::unique_lock lock{mutex};
		cond.wait(lock, [this]{ return done; });

		if (postponed_error)
			std::rethrow_exception(postponed_error);
	}

	CURL *GetEasy() noexcept {
		return request.Get();
	}

protected:
	void SetDone() {
		assert(!done);

		request.Stop();
		done = true;
		cond.notify_one();
	}

	void LockSetDone() {
		const std::scoped_lock lock{mutex};
		SetDone();
	}

private:
	/* InjectEvent callback */
	void OnDeferredStart() noexcept {
		assert(!done);

		try {
			request.Start();
		} catch (...) {
			OnError(std::current_exception());
		}
	}

	/* virtual methods from CurlResponseHandler */
	void OnError(std::exception_ptr e) noexcept final {
		const std::scoped_lock lock{mutex};
		postponed_error = std::move(e);
		SetDone();
	}
};

/**
 * The (relevant) contents of a "<D:response>" element.
 */
struct DavResponse {
	std::string href;
	unsigned status = 0;
	bool collection = false;
	std::chrono::system_clock::time_point mtime =
		std::chrono::system_clock::time_point::min();
	uint64_t length = 0;

	[[nodiscard]] bool Check() const {
		return !href.empty();
	}
};

[[gnu::pure]]
static unsigned
ParseStatus(std::string_view s) noexcept
{
	/* skip the "HTTP/1.1" prefix */
	const auto [http_1_1, rest] = Split(s, ' ');

	/* skip the string suffix */
	const auto [status_string, _] = Split(rest, ' ');

	if (const auto status = ParseInteger<unsigned>(status_string))
		return *status;

	return 0;
}

[[gnu::pure]]
static std::chrono::system_clock::time_point
ParseTimeStamp(const char *s) noexcept
{
	return std::chrono::system_clock::from_time_t(curl_getdate(s, nullptr));
}

[[gnu::pure]]
static std::chrono::system_clock::time_point
ParseTimeStamp(std::string_view s) noexcept
{
	return ParseTimeStamp(std::string{s}.c_str());
}

[[gnu::pure]]
static uint64_t
ParseU64(std::string_view s) noexcept
{
	if (const auto i = ParseInteger<uint_least64_t>(s))
		return *i;

	return 0;
}

[[gnu::pure]]
static bool
IsXmlContentType(const char *content_type) noexcept
{
	return StringStartsWith(content_type, "text/xml") ||
		StringStartsWith(content_type, "application/xml");
}

[[gnu::pure]]
static bool
IsXmlContentType(const Curl::Headers &headers) noexcept
{
	auto i = headers.find("content-type");
	return i != headers.end() && IsXmlContentType(i->second.c_str());
}

/**
 * A WebDAV PROPFIND request.  Each "response" element will be passed
 * to OnDavResponse() (to be implemented by a derived class).
 */
class PropfindOperation : BlockingHttpRequest, CommonExpatParser {
	CurlSlist request_headers;

	enum class State {
		ROOT,
		RESPONSE,
		PROPSTAT,
		HREF,
		STATUS,
		TYPE,
		MTIME,
		LENGTH,
	} state = State::ROOT;

	DavResponse response;

public:
	PropfindOperation(CurlGlobal &_curl, const char *_uri, unsigned depth)
		:BlockingHttpRequest(_curl, _uri),
		 CommonExpatParser(ExpatNamespaceSeparator{'|'})
	{
		auto &easy = request.GetEasy();

		easy.SetOption(CURLOPT_CUSTOMREQUEST, "PROPFIND");
		easy.SetOption(CURLOPT_FOLLOWLOCATION, 1L);
		easy.SetOption(CURLOPT_MAXREDIRS, 1L);

		/* this option eliminates the probe request when
		   username/password are specified */
		easy.SetOption(CURLOPT_HTTPAUTH, CURLAUTH_BASIC);

		request_headers.Append(FmtBuffer<40>("depth: {}", depth));
		request_headers.Append("content-type: text/xml");

		easy.SetRequestHeaders(request_headers.Get());

		easy.SetRequestBody("<?xml version=\"1.0\"?>\n"
				    "<a:propfind xmlns:a=\"DAV:\">"
				    "<a:prop>"
				    "<a:resourcetype/>"
				    "<a:getcontenttype/>"
				    "<a:getcontentlength/>"
				    "<a:getlastmodified/>"
				    "</a:prop>"
				    "</a:propfind>"sv);
	}

	using BlockingHttpRequest::GetEasy;
	using BlockingHttpRequest::DeferStart;
	using BlockingHttpRequest::Wait;

protected:
	virtual void OnDavResponse(DavResponse &&r) = 0;

private:
	void FinishResponse() {
		if (response.Check())
			OnDavResponse(std::move(response));
		response = DavResponse();
	}

	/* virtual methods from CurlResponseHandler */
	void OnHeaders(unsigned status, Curl::Headers &&headers) final {
		if (status != 207)
			throw HttpStatusError(status,
					      FmtBuffer<80>("Status {} from WebDAV server; expected \"207 Multi-Status\"",
							    status));

		if (!IsXmlContentType(headers))
			throw std::runtime_error("Unexpected Content-Type from WebDAV server");
	}

	void OnData(std::span<const std::byte> src) final {
		Parse(ToStringView(src));
	}

	void OnEnd() final {
		CompleteParse();
		LockSetDone();
	}

	/* virtual methods from CommonExpatParser */
	void StartElement(const XML_Char *name,
			  [[maybe_unused]] const XML_Char **attrs) final {
		switch (state) {
		case State::ROOT:
			if (strcmp(name, "DAV:|response") == 0)
				state = State::RESPONSE;
			break;

		case State::RESPONSE:
			if (strcmp(name, "DAV:|propstat") == 0)
				state = State::PROPSTAT;
			else if (strcmp(name, "DAV:|href") == 0)
				state = State::HREF;
			break;
		case State::PROPSTAT:
			if (strcmp(name, "DAV:|status") == 0)
				state = State::STATUS;
			else if (strcmp(name, "DAV:|resourcetype") == 0)
				state = State::TYPE;
			else if (strcmp(name, "DAV:|getlastmodified") == 0)
				state = State::MTIME;
			else if (strcmp(name, "DAV:|getcontentlength") == 0)
				state = State::LENGTH;
			break;

		case State::TYPE:
			if (strcmp(name, "DAV:|collection") == 0)
				response.collection = true;
			break;

		case State::HREF:
		case State::STATUS:
		case State::LENGTH:
		case State::MTIME:
			break;
		}
	}

	void EndElement(const XML_Char *name) final {
		switch (state) {
		case State::ROOT:
			break;

		case State::RESPONSE:
			if (strcmp(name, "DAV:|response") == 0) {
				state = State::ROOT;
			}
			break;

		case State::PROPSTAT:
			if (strcmp(name, "DAV:|propstat") == 0) {
				FinishResponse();
				state = State::RESPONSE;
			}

			break;

		case State::HREF:
			if (strcmp(name, "DAV:|href") == 0)
				state = State::RESPONSE;
			break;

		case State::STATUS:
			if (strcmp(name, "DAV:|status") == 0)
				state = State::PROPSTAT;
			break;

		case State::TYPE:
			if (strcmp(name, "DAV:|resourcetype") == 0)
				state = State::PROPSTAT;
			break;

		case State::MTIME:
			if (strcmp(name, "DAV:|getlastmodified") == 0)
				state = State::PROPSTAT;
			break;

		case State::LENGTH:
			if (strcmp(name, "DAV:|getcontentlength") == 0)
				state = State::PROPSTAT;
			break;
		}
	}

	void CharacterData(std::string_view s) final {
		switch (state) {
		case State::ROOT:
		case State::PROPSTAT:
		case State::RESPONSE:
		case State::TYPE:
			break;

		case State::HREF:
			response.href.append(s);
			break;

		case State::STATUS:
			response.status = ParseStatus(s);
			break;

		case State::MTIME:
			response.mtime = ParseTimeStamp(s);
			break;

		case State::LENGTH:
			response.length = ParseU64(s);
			break;
		}
	}
};

/**
 * Obtain information about a single file using WebDAV PROPFIND.
 */
class HttpGetInfoOperation final : public PropfindOperation {
	StorageFileInfo info;

public:
	HttpGetInfoOperation(CurlGlobal &curl, const char *uri)
		:PropfindOperation(curl, uri, 0),
		 info(StorageFileInfo::Type::OTHER) {
	}

	const StorageFileInfo &Perform() {
		DeferStart();
		Wait();
		return info;
	}

protected:
	/* virtual methods from PropfindOperation */
	void OnDavResponse(DavResponse &&r) override {
		if (r.status != 200)
			return;

		info.type = r.collection
			? StorageFileInfo::Type::DIRECTORY
			: StorageFileInfo::Type::REGULAR;
		info.size = r.length;
		info.mtime = r.mtime;
	}
};

StorageFileInfo
CurlStorage::GetInfo(std::string_view uri_utf8, [[maybe_unused]] bool follow)
{
	// TODO: escape the given URI

	const auto uri = MapUTF8(uri_utf8);
	return HttpGetInfoOperation(*curl, uri.c_str()).Perform();
}

[[gnu::pure]]
static std::string_view
UriPathOrSlash(const char *uri) noexcept
{
	auto path = uri_get_path(uri);
	if (path.data() == nullptr)
		path = "/";
	return path;
}

/**
 * Obtain a directory listing using WebDAV PROPFIND.
 */
class HttpListDirectoryOperation final : public PropfindOperation {
	const std::string base_path;

	MemoryStorageDirectoryReader::List entries;

public:
	HttpListDirectoryOperation(CurlGlobal &curl, const char *uri)
		:PropfindOperation(curl, uri, 1),
		 base_path(CurlUnescape(GetEasy(), UriPathOrSlash(uri))) {}

	std::unique_ptr<StorageDirectoryReader> Perform() {
		DeferStart();
		Wait();
		return ToReader();
	}

private:
	std::unique_ptr<StorageDirectoryReader> ToReader() {
		return std::make_unique<MemoryStorageDirectoryReader>(std::move(entries));
	}

	/**
	 * Convert a "href" attribute (which may be an absolute URI)
	 * to the base file name.
	 */
	[[gnu::pure]]
	std::string_view HrefToEscapedName(const char *href) const noexcept {
		std::string_view path = uri_get_path(href);
		if (path.data() == nullptr)
			return {};

		/* kludge: ignoring case in this comparison to avoid
		   false negatives if the web server uses a different
		   case */
		path = StringAfterPrefixIgnoreCase(path, base_path.c_str());
		if (path.empty())
			return {};

		const auto slash = path.find('/');
		if (slash == path.npos)
			/* regular file */
			return path;
		else if (slash + 1 == path.size())
			/* trailing slash: collection; strip the slash */
			return path.substr(0, slash);
		else
			/* strange, better ignore it */
			return {};
	}

protected:
	/* virtual methods from PropfindOperation */
	void OnDavResponse(DavResponse &&r) override {
		if (r.status != 200)
			return;

		std::string href = CurlUnescape(GetEasy(), r.href.c_str());
		const auto name = HrefToEscapedName(href.c_str());
		if (name.data() == nullptr)
			return;

		entries.emplace_front(name);

		auto &info = entries.front().info;
		info = StorageFileInfo(r.collection
				       ? StorageFileInfo::Type::DIRECTORY
				       : StorageFileInfo::Type::REGULAR);
		info.size = r.length;
		info.mtime = r.mtime;
	}
};

std::unique_ptr<StorageDirectoryReader>
CurlStorage::OpenDirectory(std::string_view uri_utf8)
{
	std::string uri = MapUTF8(uri_utf8);

	/* collection URIs must end with a slash */
	if (uri.back() != '/')
		uri.push_back('/');

	return HttpListDirectoryOperation(*curl, uri.c_str()).Perform();
}

static std::unique_ptr<Storage>
CreateCurlStorageURI(EventLoop &event_loop, const char *uri)
{
	return std::make_unique<CurlStorage>(event_loop, uri);
}

static constexpr const char *curl_prefixes[] = {
	"http://", "https://", nullptr
};

const StoragePlugin curl_storage_plugin = {
	"curl",
	curl_prefixes,
	CreateCurlStorageURI,
};
