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

#include "CurlStorage.hxx"
#include "storage/StoragePlugin.hxx"
#include "storage/StorageInterface.hxx"
#include "storage/FileInfo.hxx"
#include "storage/MemoryDirectoryReader.hxx"
#include "lib/curl/Init.hxx"
#include "lib/curl/Global.hxx"
#include "lib/curl/Slist.hxx"
#include "lib/curl/String.hxx"
#include "lib/curl/Request.hxx"
#include "lib/curl/Handler.hxx"
#include "lib/curl/Escape.hxx"
#include "lib/expat/ExpatParser.hxx"
#include "fs/Traits.hxx"
#include "event/InjectEvent.hxx"
#include "thread/Mutex.hxx"
#include "thread/Cond.hxx"
#include "time/Parser.hxx"
#include "util/ASCII.hxx"
#include "util/RuntimeError.hxx"
#include "util/StringCompare.hxx"
#include "util/StringFormat.hxx"
#include "util/UriExtract.hxx"

#include <cassert>
#include <memory>
#include <string>
#include <utility>

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
		std::unique_lock<Mutex> lock(mutex);
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
		const std::scoped_lock<Mutex> lock(mutex);
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
		const std::scoped_lock<Mutex> lock(mutex);
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

static unsigned
ParseStatus(const char *s)
{
	/* skip the "HTTP/1.1" prefix */
	const char *space = std::strchr(s, ' ');
	if (space == nullptr)
		return 0;

	return strtoul(space + 1, nullptr, 10);
}

static unsigned
ParseStatus(const char *s, size_t length)
{
	return ParseStatus(std::string(s, length).c_str());
}

static std::chrono::system_clock::time_point
ParseTimeStamp(const char *s)
{
	try {
		// TODO: make this more robust
		return ParseTimePoint(s, "%a, %d %b %Y %T");
	} catch (...) {
		return std::chrono::system_clock::time_point::min();
	}
}

static std::chrono::system_clock::time_point
ParseTimeStamp(const char *s, size_t length)
{
	return ParseTimeStamp(std::string(s, length).c_str());
}

static uint64_t
ParseU64(const char *s)
{
	return strtoull(s, nullptr, 10);
}

static uint64_t
ParseU64(const char *s, size_t length)
{
	return ParseU64(std::string(s, length).c_str());
}

gcc_pure
static bool
IsXmlContentType(const char *content_type) noexcept
{
	return StringStartsWith(content_type, "text/xml") ||
		StringStartsWith(content_type, "application/xml");
}

gcc_pure
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
		request.SetOption(CURLOPT_CUSTOMREQUEST, "PROPFIND");
		request.SetOption(CURLOPT_FOLLOWLOCATION, 1L);
		request.SetOption(CURLOPT_MAXREDIRS, 1L);

		/* this option eliminates the probe request when
		   username/password are specified */
		request.SetOption(CURLOPT_HTTPAUTH, CURLAUTH_BASIC);

		request_headers.Append(StringFormat<40>("depth: %u", depth));
		request_headers.Append("content-type: text/xml");

		request.SetOption(CURLOPT_HTTPHEADER, request_headers.Get());

		request.SetOption(CURLOPT_POSTFIELDS,
				  "<?xml version=\"1.0\"?>\n"
				  "<a:propfind xmlns:a=\"DAV:\">"
				  "<a:prop>"
				  "<a:resourcetype/>"
				  "<a:getcontenttype/>"
				  "<a:getcontentlength/>"
				  "</a:prop>"
				  "</a:propfind>");
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
			throw FormatRuntimeError("Status %d from WebDAV server; expected \"207 Multi-Status\"",
						 status);

		if (!IsXmlContentType(headers))
			throw std::runtime_error("Unexpected Content-Type from WebDAV server");
	}

	void OnData(ConstBuffer<void> _data) final {
		const auto data = ConstBuffer<char>::FromVoid(_data);
		Parse(data.data, data.size);
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

	void CharacterData(const XML_Char *s, int len) final {
		switch (state) {
		case State::ROOT:
		case State::PROPSTAT:
		case State::RESPONSE:
		case State::TYPE:
			break;

		case State::HREF:
			response.href.append(s, len);
			break;

		case State::STATUS:
			response.status = ParseStatus(s, len);
			break;

		case State::MTIME:
			response.mtime = ParseTimeStamp(s, len);
			break;

		case State::LENGTH:
			response.length = ParseU64(s, len);
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

gcc_pure
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
	gcc_pure
	StringView HrefToEscapedName(const char *href) const noexcept {
		StringView path = uri_get_path(href);
		if (path == nullptr)
			return nullptr;

		/* kludge: ignoring case in this comparison to avoid
		   false negatives if the web server uses a different
		   case */
		path = StringAfterPrefixIgnoreCase(path, base_path.c_str());
		if (path == nullptr || path.empty())
			return nullptr;

		const char *slash = path.Find('/');
		if (slash == nullptr)
			/* regular file */
			return path;
		else if (slash == &path.back())
			/* trailing slash: collection; strip the slash */
			return {path.data, slash};
		else
			/* strange, better ignore it */
			return nullptr;
	}

protected:
	/* virtual methods from PropfindOperation */
	void OnDavResponse(DavResponse &&r) override {
		if (r.status != 200)
			return;

		std::string href = CurlUnescape(GetEasy(), r.href.c_str());
		const auto name = HrefToEscapedName(href.c_str());
		if (name.IsNull())
			return;

		entries.emplace_front(std::string(name.data, name.size));

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
