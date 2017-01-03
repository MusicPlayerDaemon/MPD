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
#include "HttpdOutputPlugin.hxx"
#include "HttpdInternal.hxx"
#include "HttpdClient.hxx"
#include "output/OutputAPI.hxx"
#include "encoder/EncoderInterface.hxx"
#include "encoder/EncoderPlugin.hxx"
#include "encoder/EncoderList.hxx"
#include "net/SocketAddress.hxx"
#include "net/ToString.hxx"
#include "Page.hxx"
#include "IcyMetaDataServer.hxx"
#include "system/fd_util.h"
#include "IOThread.hxx"
#include "event/Call.hxx"
#include "util/RuntimeError.hxx"
#include "util/Domain.hxx"
#include "util/DeleteDisposer.hxx"
#include "Log.hxx"

#include <assert.h>

#include <string.h>
#include <errno.h>

#ifdef HAVE_LIBWRAP
#include <sys/socket.h> /* needed for AF_UNIX */
#include <tcpd.h>
#endif

const Domain httpd_output_domain("httpd_output");

inline
HttpdOutput::HttpdOutput(EventLoop &_loop, const ConfigBlock &block)
	:ServerSocket(_loop), DeferredMonitor(_loop),
	 base(httpd_output_plugin, block),
	 encoder(nullptr), unflushed_input(0),
	 metadata(nullptr)
{
	/* read configuration */
	name = block.GetBlockValue("name", "Set name in config");
	genre = block.GetBlockValue("genre", "Set genre in config");
	website = block.GetBlockValue("website", "Set website in config");

	unsigned port = block.GetBlockValue("port", 8000u);

	const char *encoder_name =
		block.GetBlockValue("encoder", "vorbis");
	const auto encoder_plugin = encoder_plugin_get(encoder_name);
	if (encoder_plugin == nullptr)
		throw FormatRuntimeError("No such encoder: %s", encoder_name);

	clients_max = block.GetBlockValue("max_clients", 0u);

	/* set up bind_to_address */

	const char *bind_to_address = block.GetBlockValue("bind_to_address");
	if (bind_to_address != nullptr && strcmp(bind_to_address, "any") != 0)
		AddHost(bind_to_address, port);
	else
		AddPort(port);

	/* initialize encoder */

	prepared_encoder = encoder_init(*encoder_plugin, block);

	/* determine content type */
	content_type = prepared_encoder->GetMimeType();
	if (content_type == nullptr)
		content_type = "application/octet-stream";
}

HttpdOutput::~HttpdOutput()
{
	if (metadata != nullptr)
		metadata->Unref();

	delete prepared_encoder;
}

inline void
HttpdOutput::Bind()
{
	open = false;

	BlockingCall(GetEventLoop(), [this](){
			ServerSocket::Open();
		});
}

inline void
HttpdOutput::Unbind()
{
	assert(!open);

	BlockingCall(GetEventLoop(), [this](){
			ServerSocket::Close();
		});
}

static AudioOutput *
httpd_output_init(const ConfigBlock &block)
{
	return *new HttpdOutput(io_thread_get(), block);
}

static void
httpd_output_finish(AudioOutput *ao)
{
	HttpdOutput *httpd = HttpdOutput::Cast(ao);

	delete httpd;
}

/**
 * Creates a new #HttpdClient object and adds it into the
 * HttpdOutput.clients linked list.
 */
inline void
HttpdOutput::AddClient(int fd)
{
	auto *client = new HttpdClient(*this, fd, GetEventLoop(),
				       !encoder->ImplementsTag());
	clients.push_front(*client);

	/* pass metadata to client */
	if (metadata != nullptr)
		clients.front().PushMetaData(metadata);
}

void
HttpdOutput::RunDeferred()
{
	/* this method runs in the IOThread; it broadcasts pages from
	   our own queue to all clients */

	const std::lock_guard<Mutex> protect(mutex);

	while (!pages.empty()) {
		Page *page = pages.front();
		pages.pop();

		for (auto &client : clients)
			client.PushPage(page);

		page->Unref();
	}

	/* wake up the client that may be waiting for the queue to be
	   flushed */
	cond.broadcast();
}

void
HttpdOutput::OnAccept(int fd, SocketAddress address, gcc_unused int uid)
{
	/* the listener socket has become readable - a client has
	   connected */

#ifdef HAVE_LIBWRAP
	if (address.GetFamily() != AF_UNIX) {
		const auto hostaddr = ToString(address);
		// TODO: shall we obtain the program name from argv[0]?
		const char *progname = "mpd";

		struct request_info req;
		request_init(&req, RQ_FILE, fd, RQ_DAEMON, progname, 0);

		fromhost(&req);

		if (!hosts_access(&req)) {
			/* tcp wrappers says no */
			FormatWarning(httpd_output_domain,
				      "libwrap refused connection (libwrap=%s) from %s",
				      progname, hostaddr.c_str());
			close_socket(fd);
			return;
		}
	}
#else
	(void)address;
#endif	/* HAVE_WRAP */

	const std::lock_guard<Mutex> protect(mutex);

	if (fd >= 0) {
		/* can we allow additional client */
		if (open && (clients_max == 0 || clients.size() < clients_max))
			AddClient(fd);
		else
			close_socket(fd);
	} else if (fd < 0 && errno != EINTR) {
		LogErrno(httpd_output_domain, "accept() failed");
	}
}

Page *
HttpdOutput::ReadPage()
{
	if (unflushed_input >= 65536) {
		/* we have fed a lot of input into the encoder, but it
		   didn't give anything back yet - flush now to avoid
		   buffer underruns */
		try {
			encoder->Flush();
		} catch (const std::runtime_error &) {
			/* ignore */
		}

		unflushed_input = 0;
	}

	size_t size = 0;
	do {
		size_t nbytes = encoder->Read(buffer + size,
					      sizeof(buffer) - size);
		if (nbytes == 0)
			break;

		unflushed_input = 0;

		size += nbytes;
	} while (size < sizeof(buffer));

	if (size == 0)
		return nullptr;

	return Page::Copy(buffer, size);
}

static void
httpd_output_enable(AudioOutput *ao)
{
	HttpdOutput *httpd = HttpdOutput::Cast(ao);

	httpd->Bind();
}

static void
httpd_output_disable(AudioOutput *ao)
{
	HttpdOutput *httpd = HttpdOutput::Cast(ao);

	httpd->Unbind();
}

inline void
HttpdOutput::OpenEncoder(AudioFormat &audio_format)
{
	encoder = prepared_encoder->Open(audio_format);

	/* we have to remember the encoder header, i.e. the first
	   bytes of encoder output after opening it, because it has to
	   be sent to every new client */
	header = ReadPage();

	unflushed_input = 0;
}

inline void
HttpdOutput::Open(AudioFormat &audio_format)
{
	assert(!open);
	assert(clients.empty());

	OpenEncoder(audio_format);

	/* initialize other attributes */

	timer = new Timer(audio_format);

	open = true;
}

static void
httpd_output_open(AudioOutput *ao, AudioFormat &audio_format)
{
	HttpdOutput *httpd = HttpdOutput::Cast(ao);

	const std::lock_guard<Mutex> protect(httpd->mutex);
	httpd->Open(audio_format);
}

inline void
HttpdOutput::Close()
{
	assert(open);

	open = false;

	delete timer;

	BlockingCall(GetEventLoop(), [this](){
			clients.clear_and_dispose(DeleteDisposer());
		});

	if (header != nullptr)
		header->Unref();

	delete encoder;
}

static void
httpd_output_close(AudioOutput *ao)
{
	HttpdOutput *httpd = HttpdOutput::Cast(ao);

	const std::lock_guard<Mutex> protect(httpd->mutex);
	httpd->Close();
}

void
HttpdOutput::RemoveClient(HttpdClient &client)
{
	assert(!clients.empty());

	clients.erase_and_dispose(clients.iterator_to(client),
				  DeleteDisposer());
}

void
HttpdOutput::SendHeader(HttpdClient &client) const
{
	if (header != nullptr)
		client.PushPage(header);
}

inline std::chrono::steady_clock::duration
HttpdOutput::Delay() const
{
	if (!LockHasClients() && base.pause) {
		/* if there's no client and this output is paused,
		   then httpd_output_pause() will not do anything, it
		   will not fill the buffer and it will not update the
		   timer; therefore, we reset the timer here */
		timer->Reset();

		/* some arbitrary delay that is long enough to avoid
		   consuming too much CPU, and short enough to notice
		   new clients quickly enough */
		return std::chrono::seconds(1);
	}

	return timer->IsStarted()
		? timer->GetDelay()
		: std::chrono::steady_clock::duration::zero();
}

static std::chrono::steady_clock::duration
httpd_output_delay(AudioOutput *ao)
{
	HttpdOutput *httpd = HttpdOutput::Cast(ao);

	return httpd->Delay();
}

void
HttpdOutput::BroadcastPage(Page *page)
{
	assert(page != nullptr);

	mutex.lock();
	pages.push(page);
	page->Ref();
	mutex.unlock();

	DeferredMonitor::Schedule();
}

void
HttpdOutput::BroadcastFromEncoder()
{
	/* synchronize with the IOThread */
	mutex.lock();
	while (!pages.empty())
		cond.wait(mutex);

	Page *page;
	while ((page = ReadPage()) != nullptr)
		pages.push(page);

	mutex.unlock();

	DeferredMonitor::Schedule();
}

inline void
HttpdOutput::EncodeAndPlay(const void *chunk, size_t size)
{
	encoder->Write(chunk, size);

	unflushed_input += size;

	BroadcastFromEncoder();
}

inline size_t
HttpdOutput::Play(const void *chunk, size_t size)
{
	if (LockHasClients())
		EncodeAndPlay(chunk, size);

	if (!timer->IsStarted())
		timer->Start();
	timer->Add(size);

	return size;
}

static size_t
httpd_output_play(AudioOutput *ao, const void *chunk, size_t size)
{
	HttpdOutput *httpd = HttpdOutput::Cast(ao);

	return httpd->Play(chunk, size);
}

static bool
httpd_output_pause(AudioOutput *ao)
{
	HttpdOutput *httpd = HttpdOutput::Cast(ao);

	if (httpd->LockHasClients()) {
		static const char silence[1020] = { 0 };
		httpd->Play(silence, sizeof(silence));
	}

	return true;
}

inline void
HttpdOutput::SendTag(const Tag &tag)
{
	if (encoder->ImplementsTag()) {
		/* embed encoder tags */

		/* flush the current stream, and end it */

		try {
			encoder->PreTag();
		} catch (const std::runtime_error &) {
			/* ignore */
		}

		BroadcastFromEncoder();

		/* send the tag to the encoder - which starts a new
		   stream now */

		try {
			encoder->SendTag(tag);
		} catch (const std::runtime_error &) {
			/* ignore */
		}

		/* the first page generated by the encoder will now be
		   used as the new "header" page, which is sent to all
		   new clients */

		Page *page = ReadPage();
		if (page != nullptr) {
			if (header != nullptr)
				header->Unref();
			header = page;
			BroadcastPage(page);
		}
	} else {
		/* use Icy-Metadata */

		if (metadata != nullptr)
			metadata->Unref();

		static constexpr TagType types[] = {
			TAG_ALBUM, TAG_ARTIST, TAG_TITLE,
			TAG_NUM_OF_ITEM_TYPES
		};

		metadata = icy_server_metadata_page(tag, &types[0]);
		if (metadata != nullptr) {
			const std::lock_guard<Mutex> protect(mutex);
			for (auto &client : clients)
				client.PushMetaData(metadata);
		}
	}
}

static void
httpd_output_tag(AudioOutput *ao, const Tag &tag)
{
	HttpdOutput *httpd = HttpdOutput::Cast(ao);

	httpd->SendTag(tag);
}

inline void
HttpdOutput::CancelAllClients()
{
	const std::lock_guard<Mutex> protect(mutex);

	while (!pages.empty()) {
		Page *page = pages.front();
		pages.pop();
		page->Unref();
	}

	for (auto &client : clients)
		client.CancelQueue();

	cond.broadcast();
}

static void
httpd_output_cancel(AudioOutput *ao)
{
	HttpdOutput *httpd = HttpdOutput::Cast(ao);

	BlockingCall(io_thread_get(), [httpd](){
			httpd->CancelAllClients();
		});
}

const struct AudioOutputPlugin httpd_output_plugin = {
	"httpd",
	nullptr,
	httpd_output_init,
	httpd_output_finish,
	httpd_output_enable,
	httpd_output_disable,
	httpd_output_open,
	httpd_output_close,
	httpd_output_delay,
	httpd_output_tag,
	httpd_output_play,
	nullptr,
	httpd_output_cancel,
	httpd_output_pause,
	nullptr,
};
