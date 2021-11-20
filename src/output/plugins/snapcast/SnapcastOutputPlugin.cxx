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

#include "SnapcastOutputPlugin.hxx"
#include "Internal.hxx"
#include "Client.hxx"
#include "output/OutputAPI.hxx"
#include "output/Features.h"
#include "encoder/EncoderInterface.hxx"
#include "encoder/Configured.hxx"
#include "encoder/plugins/WaveEncoderPlugin.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "net/SocketAddress.hxx"
#include "event/Call.hxx"
#include "util/Domain.hxx"
#include "util/DeleteDisposer.hxx"
#include "config/Net.hxx"

#ifdef HAVE_ZEROCONF
#include "zeroconf/Helper.hxx"
#endif

#ifdef HAVE_YAJL
#include "lib/yajl/Gen.hxx"
#endif

#include <cassert>

#include <string.h>

inline
SnapcastOutput::SnapcastOutput(EventLoop &_loop, const ConfigBlock &block)
	:AudioOutput(FLAG_ENABLE_DISABLE|FLAG_PAUSE|
		     FLAG_NEED_FULLY_DEFINED_AUDIO_FORMAT),
	 ServerSocket(_loop),
	 inject_event(_loop, BIND_THIS_METHOD(OnInject)),
	 // TODO: support other encoder plugins?
	 prepared_encoder(encoder_init(wave_encoder_plugin, block))
{
	const unsigned port = block.GetBlockValue("port", 1704U);
	ServerSocketAddGeneric(*this, block.GetBlockValue("bind_to_address"),
			       port);

#ifdef HAVE_ZEROCONF
	if (block.GetBlockValue("zeroconf", true))
		zeroconf_port = port;
#endif
}

SnapcastOutput::~SnapcastOutput() noexcept = default;

inline void
SnapcastOutput::Bind()
{
	open = false;

	BlockingCall(GetEventLoop(), [this](){
		ServerSocket::Open();

#ifdef HAVE_ZEROCONF
		if (zeroconf_port > 0)
			zeroconf_helper = std::make_unique<ZeroconfHelper>
				(GetEventLoop(), "Music Player Daemon",
				 "_snapcast._tcp", zeroconf_port);
#endif
	});
}

inline void
SnapcastOutput::Unbind() noexcept
{
	assert(!open);

	BlockingCall(GetEventLoop(), [this](){
#ifdef HAVE_ZEROCONF
		zeroconf_helper.reset();
#endif

		ServerSocket::Close();
	});
}

/**
 * Creates a new #SnapcastClient object and adds it into the
 * SnapcastOutput.clients linked list.
 */
inline void
SnapcastOutput::AddClient(UniqueSocketDescriptor fd) noexcept
{
	auto *client = new SnapcastClient(*this, std::move(fd));
	clients.push_front(*client);
}

void
SnapcastOutput::OnAccept(UniqueSocketDescriptor fd,
			 SocketAddress, int) noexcept
{
	/* the listener socket has become readable - a client has
	   connected */

	const std::scoped_lock<Mutex> protect(mutex);

	/* can we allow additional client */
	if (open)
		AddClient(std::move(fd));
}

static AllocatedArray<std::byte>
ReadEncoder(Encoder &encoder)
{
	std::byte buffer[4096];

	size_t nbytes = encoder.Read(buffer, sizeof(buffer));
	const ConstBuffer<std::byte> src(buffer, nbytes);
	return AllocatedArray<std::byte>{src};
}

inline void
SnapcastOutput::OpenEncoder(AudioFormat &audio_format)
{
	encoder = prepared_encoder->Open(audio_format);

	try {
		codec_header = ReadEncoder(*encoder);
	} catch (...) {
		delete encoder;
		throw;
	}

	unflushed_input = 0;
}

void
SnapcastOutput::Open(AudioFormat &audio_format)
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
SnapcastOutput::Close() noexcept
{
	assert(open);

	delete timer;

	BlockingCall(GetEventLoop(), [this](){
		inject_event.Cancel();

		const std::scoped_lock<Mutex> protect(mutex);
		open = false;
		clients.clear_and_dispose(DeleteDisposer{});
	});

	ClearQueue(chunks);

	codec_header = nullptr;
	delete encoder;
}

void
SnapcastOutput::OnInject() noexcept
{
	const std::scoped_lock<Mutex> protect(mutex);

	while (!chunks.empty()) {
		const auto chunk = std::move(chunks.front());
		chunks.pop();

		for (auto &client : clients)
			client.Push(chunk);
	}
}

void
SnapcastOutput::RemoveClient(SnapcastClient &client) noexcept
{
	assert(!clients.empty());

	client.unlink();
	delete &client;

	if (clients.empty())
		drain_cond.notify_one();
}

std::chrono::steady_clock::duration
SnapcastOutput::Delay() const noexcept
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

#ifdef HAVE_YAJL

static constexpr struct {
	TagType type;
	const char *name;
} snapcast_tags[] = {
	/* these tags are mentioned in an example in
	   snapcast/common/message/stream_tags.hpp */
	{ TAG_ARTIST, "artist" },
	{ TAG_ALBUM, "album" },
	{ TAG_TITLE, "track" },
	{ TAG_MUSICBRAINZ_TRACKID, "musicbrainzid" },
};

static bool
TranslateTagType(Yajl::Gen &gen, const Tag &tag, TagType type,
		 const char *name) noexcept
{
	// TODO: support multiple values?
	const char *value = tag.GetValue(type);
	if (value == nullptr)
		return false;

	gen.String(name);
	gen.String(value);
	return true;
}

static std::string
ToJson(const Tag &tag) noexcept
{
	Yajl::Gen gen(nullptr);
	gen.OpenMap();

	bool empty = true;

	for (const auto [type, name] : snapcast_tags)
		if (TranslateTagType(gen, tag, type, name))
			empty = false;

	if (empty)
		return {};

	gen.CloseMap();

	const auto result = gen.GetBuffer();
	return {(const char *)result.data, result.size};
}

#endif

void
SnapcastOutput::SendTag(const Tag &tag)
{
#ifdef HAVE_YAJL
	if (!LockHasClients())
		return;

	const auto json = ToJson(tag);
	if (json.empty())
		return;

	const ConstBuffer payload(json.data(), json.size());

	const std::scoped_lock<Mutex> protect(mutex);
	// TODO: enqueue StreamTags, don't send directly
	for (auto &client : clients)
		client.SendStreamTags(payload.ToVoid());
#else
	(void)tag;
#endif
}

size_t
SnapcastOutput::Play(const void *chunk, size_t size)
{
	pause = false;

	const auto now = std::chrono::steady_clock::now();

	if (!timer->IsStarted())
		timer->Start();
	timer->Add(size);

	if (!LockHasClients())
		return size;

	encoder->Write(chunk, size);
	unflushed_input += size;

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

	while (true) {
		std::byte buffer[32768];

		size_t nbytes = encoder->Read(buffer, sizeof(buffer));
		if (nbytes == 0)
			break;

		unflushed_input = 0;

		const std::scoped_lock<Mutex> protect(mutex);
		if (chunks.empty())
			inject_event.Schedule();

		const ConstBuffer payload{buffer, nbytes};
		chunks.push(std::make_shared<SnapcastChunk>(now, AllocatedArray{payload}));
	}

	return size;
}

bool
SnapcastOutput::Pause()
{
	pause = true;

	return true;
}

inline bool
SnapcastOutput::IsDrained() const noexcept
{
	if (!chunks.empty())
		return false;

	return std::all_of(clients.begin(), clients.end(), [](auto&& c){ return c.IsDrained(); });
}

void
SnapcastOutput::Drain()
{
	std::unique_lock<Mutex> protect(mutex);
	drain_cond.wait(protect, [this]{ return IsDrained(); });
}

void
SnapcastOutput::Cancel() noexcept
{
	const std::scoped_lock<Mutex> protect(mutex);

	ClearQueue(chunks);

	for (auto &client : clients)
		client.Cancel();
}

const struct AudioOutputPlugin snapcast_output_plugin = {
	"snapcast",
	nullptr,
	&SnapcastOutput::Create,
	nullptr,
};
