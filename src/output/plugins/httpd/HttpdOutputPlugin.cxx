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

#include "HttpdOutputPlugin.hxx"
#include "HttpdInternal.hxx"
#include "HttpdClient.hxx"
#include "output/OutputAPI.hxx"
#include "encoder/EncoderInterface.hxx"
#include "encoder/Configured.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "net/SocketAddress.hxx"
#include "Page.hxx"
#include "IcyMetaDataServer.hxx"
#include "event/Call.hxx"
#include "net/DscpParser.hxx"
#include "util/Domain.hxx"
#include "util/DeleteDisposer.hxx"
#include "config/Net.hxx"

#include <cassert>
#include <stdexcept>

#include <string.h>

const Domain httpd_output_domain("httpd_output");

inline
HttpdOutput::HttpdOutput(EventLoop &_loop, const ConfigBlock &block)
	:AudioOutput(FLAG_ENABLE_DISABLE|FLAG_PAUSE),
	 ServerSocket(_loop),
	 prepared_encoder(CreateConfiguredEncoder(block)),
	 defer_broadcast(_loop, BIND_THIS_METHOD(OnDeferredBroadcast)),
	 name(block.GetBlockValue("name", "Set name in config")),
	 genre(block.GetBlockValue("genre", "Set genre in config")),
	 website(block.GetBlockValue("website", "Set website in config")),
	 clients_max(block.GetBlockValue("max_clients", 0U))
{
	if (const auto *p = block.GetBlockParam("dscp_class"))
		p->With([this](const char *s){
			const int value = ParseDscpClass(s);
			if (value < 0)
				throw std::runtime_error("Not a valid DSCP class");

			ServerSocket::SetDscpClass(value);
		});

	/* set up bind_to_address */

	ServerSocketAddGeneric(*this, block.GetBlockValue("bind_to_address"), block.GetBlockValue("port", 8000U));

	/* determine content type */
	content_type = prepared_encoder->GetMimeType();
	if (content_type == nullptr)
		content_type = "application/octet-stream";
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
HttpdOutput::Unbind() noexcept
{
	assert(!open);

	BlockingCall(GetEventLoop(), [this](){
			ServerSocket::Close();
		});
}

/**
 * Creates a new #HttpdClient object and adds it into the
 * HttpdOutput.clients linked list.
 */
inline void
HttpdOutput::AddClient(UniqueSocketDescriptor fd) noexcept
{
	auto *client = new HttpdClient(*this, std::move(fd), GetEventLoop(),
				       !encoder->ImplementsTag());
	clients.push_front(*client);

	/* pass metadata to client */
	if (metadata != nullptr)
		clients.front().PushMetaData(metadata);
}

void
HttpdOutput::OnDeferredBroadcast() noexcept
{
	/* this method runs in the IOThread; it broadcasts pages from
	   our own queue to all clients */

	const std::scoped_lock<Mutex> protect(mutex);

	while (!pages.empty()) {
		PagePtr page = std::move(pages.front());
		pages.pop();

		for (auto &client : clients)
			client.PushPage(page);
	}

	/* wake up the client that may be waiting for the queue to be
	   flushed */
	cond.notify_all();
}

void
HttpdOutput::OnAccept(UniqueSocketDescriptor fd,
		      SocketAddress, [[maybe_unused]] int uid) noexcept
{
	/* the listener socket has become readable - a client has
	   connected */

	const std::scoped_lock<Mutex> protect(mutex);

	/* can we allow additional client */
	if (open && (clients_max == 0 || clients.size() < clients_max))
		AddClient(std::move(fd));
}

PagePtr
HttpdOutput::ReadPage()
{
	if (unflushed_input >= 65536) {
		/* we have fed a lot of input into the encoder, but it
		   didn't give anything back yet - flush now to avoid
		   buffer underruns */
		try {
			encoder->Flush();
		} catch (...) {
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

	return std::make_shared<Page>(ConstBuffer{buffer, size});
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

void
HttpdOutput::Open(AudioFormat &audio_format)
{
	assert(!open);
	assert(clients.empty());

	const std::scoped_lock<Mutex> protect(mutex);

	OpenEncoder(audio_format);

	/* initialize other attributes */

	timer = new Timer(audio_format);

	open = true;
	pause = false;
}

void
HttpdOutput::Close() noexcept
{
	assert(open);

	delete timer;

	BlockingCall(GetEventLoop(), [this](){
			defer_broadcast.Cancel();

			const std::scoped_lock<Mutex> protect(mutex);
			open = false;
			clients.clear_and_dispose(DeleteDisposer());
		});

	header.reset();

	delete encoder;
}

void
HttpdOutput::RemoveClient(HttpdClient &client) noexcept
{
	assert(!clients.empty());

	clients.erase_and_dispose(clients.iterator_to(client),
				  DeleteDisposer());
}

void
HttpdOutput::SendHeader(HttpdClient &client) const noexcept
{
	if (header != nullptr)
		client.PushPage(header);
}

std::chrono::steady_clock::duration
HttpdOutput::Delay() const noexcept
{
	if (!LockHasClients() && pause) {
		/* if there's no client and this output is paused,
		   then Pause() will not do anything, it will not fill
		   the buffer and it will not update the timer;
		   therefore, we reset the timer here */
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

void
HttpdOutput::BroadcastPage(PagePtr page) noexcept
{
	assert(page != nullptr);

	{
		const std::scoped_lock<Mutex> lock(mutex);
		pages.emplace(std::move(page));
	}

	defer_broadcast.Schedule();
}

void
HttpdOutput::BroadcastFromEncoder()
{
	/* synchronize with the IOThread */
	{
		std::unique_lock<Mutex> lock(mutex);
		cond.wait(lock, [this]{ return pages.empty(); });
	}

	bool empty = true;

	PagePtr page;
	while ((page = ReadPage()) != nullptr) {
		const std::scoped_lock<Mutex> lock(mutex);
		pages.emplace(std::move(page));
		empty = false;
	}

	if (!empty)
		defer_broadcast.Schedule();
}

inline void
HttpdOutput::EncodeAndPlay(const void *chunk, size_t size)
{
	encoder->Write(chunk, size);

	unflushed_input += size;

	BroadcastFromEncoder();
}

size_t
HttpdOutput::Play(const void *chunk, size_t size)
{
	pause = false;

	if (LockHasClients())
		EncodeAndPlay(chunk, size);

	if (!timer->IsStarted())
		timer->Start();
	timer->Add(size);

	return size;
}

bool
HttpdOutput::Pause()
{
	pause = true;

	if (LockHasClients()) {
		static const char silence[1020] = { 0 };
		Play(silence, sizeof(silence));
	}

	return true;
}

void
HttpdOutput::SendTag(const Tag &tag)
{
	if (encoder->ImplementsTag()) {
		/* embed encoder tags */

		/* flush the current stream, and end it */

		try {
			encoder->PreTag();
		} catch (...) {
			/* ignore */
		}

		BroadcastFromEncoder();

		/* send the tag to the encoder - which starts a new
		   stream now */

		try {
			encoder->SendTag(tag);
			encoder->Flush();
		} catch (...) {
			/* ignore */
		}

		/* the first page generated by the encoder will now be
		   used as the new "header" page, which is sent to all
		   new clients */

		auto page = ReadPage();
		if (page != nullptr) {
			header = page;
			BroadcastPage(page);
		}
	} else {
		/* use Icy-Metadata */

		static constexpr TagType types[] = {
			TAG_ALBUM, TAG_ARTIST, TAG_TITLE,
			TAG_NUM_OF_ITEM_TYPES
		};

		metadata = icy_server_metadata_page(tag, &types[0]);
		if (metadata != nullptr) {
			const std::scoped_lock<Mutex> protect(mutex);
			for (auto &client : clients)
				client.PushMetaData(metadata);
		}
	}
}

inline void
HttpdOutput::CancelAllClients() noexcept
{
	const std::scoped_lock<Mutex> protect(mutex);

	while (!pages.empty()) {
		PagePtr page = std::move(pages.front());
		pages.pop();
	}

	for (auto &client : clients)
		client.CancelQueue();

	cond.notify_all();
}

void
HttpdOutput::Cancel() noexcept
{
	BlockingCall(GetEventLoop(), [this](){
			CancelAllClients();
		});
}

const struct AudioOutputPlugin httpd_output_plugin = {
	"httpd",
	nullptr,
	&HttpdOutput::Create,
	nullptr,
};
