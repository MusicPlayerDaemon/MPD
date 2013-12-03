/*
 * Copyright (C) 2003-2013 The Music Player Daemon Project
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
#include "InputStream.hxx"
#include "InputPlugin.hxx"
#include "ConfigGlobal.hxx"
#include "ConfigData.hxx"
#include "tag/Tag.hxx"
#include "tag/TagBuilder.hxx"
#include "IcyMetaDataParser.hxx"
#include "event/SocketMonitor.hxx"
#include "event/TimeoutMonitor.hxx"
#include "event/Call.hxx"
#include "IOThread.hxx"
#include "util/ASCII.hxx"
#include "util/CharUtil.hxx"
#include "util/NumberParser.hxx"
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
#include <errno.h>

#include <list>

#include <curl/curl.h>
#include <glib.h>

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

/**
 * Buffers created by input_curl_writefunction().
 */
class CurlInputBuffer {
	/** size of the payload */
	size_t size;

	/** how much has been consumed yet? */
	size_t consumed;

	/** the payload */
	uint8_t *data;

public:
	CurlInputBuffer(const void *_data, size_t _size)
		:size(_size), consumed(0), data(new uint8_t[size]) {
		memcpy(data, _data, size);
	}

	~CurlInputBuffer() {
		delete[] data;
	}

	CurlInputBuffer(const CurlInputBuffer &) = delete;
	CurlInputBuffer &operator=(const CurlInputBuffer &) = delete;

	const void *Begin() const {
		return data + consumed;
	}

	size_t TotalSize() const {
		return size;
	}

	size_t Available() const {
		return size - consumed;
	}

	/**
	 * Mark a part of the buffer as consumed.
	 *
	 * @return false if the buffer is now empty
	 */
	bool Consume(size_t length) {
		assert(consumed < size);

		consumed += length;
		if (consumed < size)
			return true;

		assert(consumed == size);
		return false;
	}

	bool Read(void *dest, size_t length) {
		assert(consumed + length <= size);

		memcpy(dest, data + consumed, length);
		return Consume(length);
	}
};

struct input_curl {
	InputStream base;

	/* some buffers which were passed to libcurl, which we have
	   too free */
	char range[32];
	struct curl_slist *request_headers;

	/** the curl handles */
	CURL *easy;

	/** list of buffers, where input_curl_writefunction() appends
	    to, and input_curl_read() reads from them */
	std::list<CurlInputBuffer> buffers;

	/**
	 * Is the connection currently paused?  That happens when the
	 * buffer was getting too large.  It will be unpaused when the
	 * buffer is below the threshold again.
	 */
	bool paused;

	/** error message provided by libcurl */
	char error[CURL_ERROR_SIZE];

	/** parser for icy-metadata */
	IcyMetaDataParser icy;

	/** the stream name from the icy-name response header */
	std::string meta_name;

	/** the tag object ready to be requested via
	    InputStream::ReadTag() */
	Tag *tag;

	Error postponed_error;

	input_curl(const char *url, Mutex &mutex, Cond &cond)
		:base(input_plugin_curl, url, mutex, cond),
		 request_headers(nullptr),
		 paused(false),
		 tag(nullptr) {}

	~input_curl();

	input_curl(const input_curl &) = delete;
	input_curl &operator=(const input_curl &) = delete;
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

		Steal();
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

	bool Add(input_curl *c, Error &error);
	void Remove(input_curl *c);

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
static struct input_curl *
input_curl_find_request(CURL *easy)
{
	assert(io_thread_inside());

	void *p;
	CURLcode code = curl_easy_getinfo(easy, CURLINFO_PRIVATE, &p);
	if (code != CURLE_OK)
		return nullptr;

	return (input_curl *)p;
}

static void
input_curl_resume(struct input_curl *c)
{
	assert(io_thread_inside());

	if (c->paused) {
		c->paused = false;
		curl_easy_pause(c->easy, CURLPAUSE_CONT);

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
CurlMulti::Add(struct input_curl *c, Error &error)
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
input_curl_easy_add_indirect(struct input_curl *c, Error &error)
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
CurlMulti::Remove(input_curl *c)
{
	curl_multi_remove_handle(multi, c->easy);
}

/**
 * Frees the current "libcurl easy" handle, and everything associated
 * with it.
 *
 * Runs in the I/O thread.
 */
static void
input_curl_easy_free(struct input_curl *c)
{
	assert(io_thread_inside());
	assert(c != nullptr);

	if (c->easy == nullptr)
		return;

	curl_multi->Remove(c);

	curl_easy_cleanup(c->easy);
	c->easy = nullptr;

	curl_slist_free_all(c->request_headers);
	c->request_headers = nullptr;
}

/**
 * Frees the current "libcurl easy" handle, and everything associated
 * with it.
 *
 * The mutex must not be locked.
 */
static void
input_curl_easy_free_indirect(struct input_curl *c)
{
	BlockingCall(io_thread_get(), [c](){
			input_curl_easy_free(c);
			curl_multi->InvalidateSockets();
		});

	assert(c->easy == nullptr);
}

/**
 * A HTTP request is finished.
 *
 * Runs in the I/O thread.  The caller must not hold locks.
 */
static void
input_curl_request_done(struct input_curl *c, CURLcode result, long status)
{
	assert(io_thread_inside());
	assert(c != nullptr);
	assert(c->easy == nullptr);
	assert(!c->postponed_error.IsDefined());

	const ScopeLock protect(c->base.mutex);

	if (result != CURLE_OK) {
		c->postponed_error.Format(curl_domain, result,
					  "curl failed: %s", c->error);
	} else if (status < 200 || status >= 300) {
		c->postponed_error.Format(http_domain, status,
					  "got HTTP status %ld",
					  status);
	}

	c->base.ready = true;

	c->base.cond.broadcast();
}

static void
input_curl_handle_done(CURL *easy_handle, CURLcode result)
{
	struct input_curl *c = input_curl_find_request(easy_handle);
	assert(c != nullptr);

	long status = 0;
	curl_easy_getinfo(easy_handle, CURLINFO_RESPONSE_CODE, &status);

	input_curl_easy_free(c);
	input_curl_request_done(c, result, status);
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

static bool
input_curl_init(const config_param &param, Error &error)
{
	CURLcode code = curl_global_init(CURL_GLOBAL_ALL);
	if (code != CURLE_OK) {
		error.Format(curl_domain, code,
			     "curl_global_init() failed: %s",
			     curl_easy_strerror(code));
		return false;
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
		error.Set(curl_domain, 0, "curl_multi_init() failed");
		return false;
	}

	curl_multi = new CurlMulti(io_thread_get(), multi);
	return true;
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

/**
 * Determine the total sizes of all buffers, including portions that
 * have already been consumed.
 *
 * The caller must lock the mutex.
 */
gcc_pure
static size_t
curl_total_buffer_size(const struct input_curl *c)
{
	size_t total = 0;

	for (const auto &i : c->buffers)
		total += i.TotalSize();

	return total;
}

input_curl::~input_curl()
{
	delete tag;

	input_curl_easy_free_indirect(this);
}

static bool
input_curl_check(InputStream *is, Error &error)
{
	struct input_curl *c = (struct input_curl *)is;

	bool success = !c->postponed_error.IsDefined();
	if (!success) {
		error = std::move(c->postponed_error);
		c->postponed_error.Clear();
	}

	return success;
}

static Tag *
input_curl_tag(InputStream *is)
{
	struct input_curl *c = (struct input_curl *)is;
	Tag *tag = c->tag;

	c->tag = nullptr;
	return tag;
}

static bool
fill_buffer(struct input_curl *c, Error &error)
{
	while (c->easy != nullptr && c->buffers.empty())
		c->base.cond.wait(c->base.mutex);

	if (c->postponed_error.IsDefined()) {
		error = std::move(c->postponed_error);
		c->postponed_error.Clear();
		return false;
	}

	return !c->buffers.empty();
}

static size_t
read_from_buffer(IcyMetaDataParser &icy, std::list<CurlInputBuffer> &buffers,
		 void *dest0, size_t length)
{
	auto &buffer = buffers.front();
	uint8_t *dest = (uint8_t *)dest0;
	size_t nbytes = 0;

	if (length > buffer.Available())
		length = buffer.Available();

	while (true) {
		size_t chunk;

		chunk = icy.Data(length);
		if (chunk > 0) {
			const bool empty = !buffer.Read(dest, chunk);

			nbytes += chunk;
			dest += chunk;
			length -= chunk;

			if (empty) {
				buffers.pop_front();
				break;
			}

			if (length == 0)
				break;
		}

		chunk = icy.Meta(buffer.Begin(), length);
		if (chunk > 0) {
			const bool empty = !buffer.Consume(chunk);

			length -= chunk;

			if (empty) {
				buffers.pop_front();
				break;
			}

			if (length == 0)
				break;
		}
	}

	return nbytes;
}

static void
copy_icy_tag(struct input_curl *c)
{
	Tag *tag = c->icy.ReadTag();

	if (tag == nullptr)
		return;

	delete c->tag;

	if (!c->meta_name.empty() && !tag->HasType(TAG_NAME)) {
		TagBuilder tag_builder(std::move(*tag));
		tag_builder.AddItem(TAG_NAME, c->meta_name.c_str());
		tag_builder.Commit(*tag);
	}

	c->tag = tag;
}

static bool
input_curl_available(InputStream *is)
{
	struct input_curl *c = (struct input_curl *)is;

	return c->postponed_error.IsDefined() || c->easy == nullptr ||
		!c->buffers.empty();
}

static size_t
input_curl_read(InputStream *is, void *ptr, size_t size,
		Error &error)
{
	struct input_curl *c = (struct input_curl *)is;
	bool success;
	size_t nbytes = 0;
	char *dest = (char *)ptr;

	do {
		/* fill the buffer */

		success = fill_buffer(c, error);
		if (!success)
			return 0;

		/* send buffer contents */

		while (size > 0 && !c->buffers.empty()) {
			size_t copy = read_from_buffer(c->icy, c->buffers,
						       dest + nbytes, size);

			nbytes += copy;
			size -= copy;
		}
	} while (nbytes == 0);

	if (c->icy.IsDefined())
		copy_icy_tag(c);

	is->offset += (InputPlugin::offset_type)nbytes;

	if (c->paused && curl_total_buffer_size(c) < CURL_RESUME_AT) {
		c->base.mutex.unlock();

		BlockingCall(io_thread_get(), [c](){
				input_curl_resume(c);
			});

		c->base.mutex.lock();
	}

	return nbytes;
}

static void
input_curl_close(InputStream *is)
{
	struct input_curl *c = (struct input_curl *)is;

	delete c;
}

static bool
input_curl_eof(gcc_unused InputStream *is)
{
	struct input_curl *c = (struct input_curl *)is;

	return c->easy == nullptr && c->buffers.empty();
}

/** called by curl when new data is available */
static size_t
input_curl_headerfunction(void *ptr, size_t size, size_t nmemb, void *stream)
{
	struct input_curl *c = (struct input_curl *)stream;
	char name[64];

	size *= nmemb;

	const char *header = (const char *)ptr;
	const char *end = header + size;

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

	if (StringEqualsCaseASCII(name, "accept-ranges")) {
		/* a stream with icy-metadata is not seekable */
		if (!c->icy.IsDefined())
			c->base.seekable = true;
	} else if (StringEqualsCaseASCII(name, "content-length")) {
		char buffer[64];

		if ((size_t)(end - header) >= sizeof(buffer))
			return size;

		memcpy(buffer, value, end - value);
		buffer[end - value] = 0;

		c->base.size = c->base.offset + ParseUint64(buffer);
	} else if (StringEqualsCaseASCII(name, "content-type")) {
		c->base.mime.assign(value, end);
	} else if (StringEqualsCaseASCII(name, "icy-name") ||
		   StringEqualsCaseASCII(name, "ice-name") ||
		   StringEqualsCaseASCII(name, "x-audiocast-name")) {
		c->meta_name.assign(value, end);

		delete c->tag;

		TagBuilder tag_builder;
		tag_builder.AddItem(TAG_NAME, c->meta_name.c_str());

		c->tag = tag_builder.Commit();
	} else if (StringEqualsCaseASCII(name, "icy-metaint")) {
		char buffer[64];
		size_t icy_metaint;

		if ((size_t)(end - header) >= sizeof(buffer) ||
		    c->icy.IsDefined())
			return size;

		memcpy(buffer, value, end - value);
		buffer[end - value] = 0;

		icy_metaint = ParseUint64(buffer);
		FormatDebug(curl_domain, "icy-metaint=%zu", icy_metaint);

		if (icy_metaint > 0) {
			c->icy.Start(icy_metaint);

			/* a stream with icy-metadata is not
			   seekable */
			c->base.seekable = false;
		}
	}

	return size;
}

/** called by curl when new data is available */
static size_t
input_curl_writefunction(void *ptr, size_t size, size_t nmemb, void *stream)
{
	struct input_curl *c = (struct input_curl *)stream;

	size *= nmemb;
	if (size == 0)
		return 0;

	const ScopeLock protect(c->base.mutex);

	if (curl_total_buffer_size(c) + size >= CURL_MAX_BUFFERED) {
		c->paused = true;
		return CURL_WRITEFUNC_PAUSE;
	}

	c->buffers.emplace_back(ptr, size);
	c->base.ready = true;

	c->base.cond.broadcast();
	return size;
}

static bool
input_curl_easy_init(struct input_curl *c, Error &error)
{
	CURLcode code;

	c->easy = curl_easy_init();
	if (c->easy == nullptr) {
		error.Set(curl_domain, "curl_easy_init() failed");
		return false;
	}

	curl_easy_setopt(c->easy, CURLOPT_PRIVATE, (void *)c);
	curl_easy_setopt(c->easy, CURLOPT_USERAGENT,
			 "Music Player Daemon " VERSION);
	curl_easy_setopt(c->easy, CURLOPT_HEADERFUNCTION,
			 input_curl_headerfunction);
	curl_easy_setopt(c->easy, CURLOPT_WRITEHEADER, c);
	curl_easy_setopt(c->easy, CURLOPT_WRITEFUNCTION,
			 input_curl_writefunction);
	curl_easy_setopt(c->easy, CURLOPT_WRITEDATA, c);
	curl_easy_setopt(c->easy, CURLOPT_HTTP200ALIASES, http_200_aliases);
	curl_easy_setopt(c->easy, CURLOPT_FOLLOWLOCATION, 1);
	curl_easy_setopt(c->easy, CURLOPT_NETRC, 1);
	curl_easy_setopt(c->easy, CURLOPT_MAXREDIRS, 5);
	curl_easy_setopt(c->easy, CURLOPT_FAILONERROR, true);
	curl_easy_setopt(c->easy, CURLOPT_ERRORBUFFER, c->error);
	curl_easy_setopt(c->easy, CURLOPT_NOPROGRESS, 1l);
	curl_easy_setopt(c->easy, CURLOPT_NOSIGNAL, 1l);
	curl_easy_setopt(c->easy, CURLOPT_CONNECTTIMEOUT, 10l);

	if (proxy != nullptr)
		curl_easy_setopt(c->easy, CURLOPT_PROXY, proxy);

	if (proxy_port > 0)
		curl_easy_setopt(c->easy, CURLOPT_PROXYPORT, (long)proxy_port);

	if (proxy_user != nullptr && proxy_password != nullptr) {
		char proxy_auth_str[1024];
		snprintf(proxy_auth_str, sizeof(proxy_auth_str),
			 "%s:%s",
			 proxy_user, proxy_password);
		curl_easy_setopt(c->easy, CURLOPT_PROXYUSERPWD, proxy_auth_str);
	}

	code = curl_easy_setopt(c->easy, CURLOPT_URL, c->base.uri.c_str());
	if (code != CURLE_OK) {
		error.Format(curl_domain, code,
			     "curl_easy_setopt() failed: %s",
			     curl_easy_strerror(code));
		return false;
	}

	c->request_headers = nullptr;
	c->request_headers = curl_slist_append(c->request_headers,
					       "Icy-Metadata: 1");
	curl_easy_setopt(c->easy, CURLOPT_HTTPHEADER, c->request_headers);

	return true;
}

static bool
input_curl_seek(InputStream *is, InputPlugin::offset_type offset,
		int whence,
		Error &error)
{
	struct input_curl *c = (struct input_curl *)is;
	bool ret;

	assert(is->ready);

	if (whence == SEEK_SET && offset == is->offset)
		/* no-op */
		return true;

	if (!is->seekable)
		return false;

	/* calculate the absolute offset */

	switch (whence) {
	case SEEK_SET:
		break;

	case SEEK_CUR:
		offset += is->offset;
		break;

	case SEEK_END:
		if (is->size < 0)
			/* stream size is not known */
			return false;

		offset += is->size;
		break;

	default:
		return false;
	}

	if (offset < 0)
		return false;

	/* check if we can fast-forward the buffer */

	while (offset > is->offset && !c->buffers.empty()) {
		auto &buffer = c->buffers.front();
		size_t length = buffer.Available();
		if (offset - is->offset < (InputPlugin::offset_type)length)
			length = offset - is->offset;

		const bool empty = !buffer.Consume(length);
		if (empty)
			c->buffers.pop_front();

		is->offset += length;
	}

	if (offset == is->offset)
		return true;

	/* close the old connection and open a new one */

	c->base.mutex.unlock();

	input_curl_easy_free_indirect(c);
	c->buffers.clear();

	is->offset = offset;
	if (is->offset == is->size) {
		/* seek to EOF: simulate empty result; avoid
		   triggering a "416 Requested Range Not Satisfiable"
		   response */
		return true;
	}

	ret = input_curl_easy_init(c, error);
	if (!ret)
		return false;

	/* send the "Range" header */

	if (is->offset > 0) {
		sprintf(c->range, "%lld-", (long long)is->offset);
		curl_easy_setopt(c->easy, CURLOPT_RANGE, c->range);
	}

	c->base.ready = false;

	if (!input_curl_easy_add_indirect(c, error))
		return false;

	c->base.mutex.lock();

	while (!c->base.ready)
		c->base.cond.wait(c->base.mutex);

	if (c->postponed_error.IsDefined()) {
		error = std::move(c->postponed_error);
		c->postponed_error.Clear();
		return false;
	}

	return true;
}

static InputStream *
input_curl_open(const char *url, Mutex &mutex, Cond &cond,
		Error &error)
{
	if (memcmp(url, "http://",  7) != 0 &&
	    memcmp(url, "https://", 8) != 0)
		return nullptr;

	struct input_curl *c = new input_curl(url, mutex, cond);

	if (!input_curl_easy_init(c, error)) {
		delete c;
		return nullptr;
	}

	if (!input_curl_easy_add_indirect(c, error)) {
		delete c;
		return nullptr;
	}

	return &c->base;
}

const struct InputPlugin input_plugin_curl = {
	"curl",
	input_curl_init,
	input_curl_finish,
	input_curl_open,
	input_curl_close,
	input_curl_check,
	nullptr,
	input_curl_tag,
	input_curl_available,
	input_curl_read,
	input_curl_eof,
	input_curl_seek,
};
