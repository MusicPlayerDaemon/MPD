// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Client.hxx"
#include "Protocol.hxx"
#include "Timestamp.hxx"
#include "Internal.hxx"
#include "tag/RiffFormat.hxx"
#include "event/Loop.hxx"
#include "net/SocketError.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "util/PackedBigEndian.hxx"
#include "util/PackedLittleEndian.hxx"
#include "util/SpanCast.hxx"
#include "Log.hxx"

#include <cassert>
#include <cstring>
#include <string_view>

SnapcastClient::SnapcastClient(SnapcastOutput &_output,
			       UniqueSocketDescriptor _fd) noexcept
	:BufferedSocket(_fd.Release(), _output.GetEventLoop()),
	 output(_output)
{
}

SnapcastClient::~SnapcastClient() noexcept
{
	if (IsDefined())
		BufferedSocket::Close();
}

void
SnapcastClient::Close() noexcept
{
	output.RemoveClient(*this);
}

void
SnapcastClient::LockClose() noexcept
{
	const std::scoped_lock protect{output.mutex};
	Close();
}

void
SnapcastClient::Push(SnapcastChunkPtr chunk) noexcept
{
	if (!active)
		return;

	chunks.emplace(std::move(chunk));
	event.ScheduleWrite();
}

SnapcastChunkPtr
SnapcastClient::LockPopQueue() noexcept
{
	const std::scoped_lock protect{output.mutex};
	if (chunks.empty())
		return nullptr;

	auto chunk = std::move(chunks.front());
	chunks.pop();

	if (chunks.empty())
		output.drain_cond.notify_one();

	return chunk;
}

void
SnapcastClient::OnSocketReady(unsigned flags) noexcept
{
	if (flags & SocketEvent::WRITE) {
		constexpr auto max_age = std::chrono::milliseconds(500);
		const auto min_time = GetEventLoop().SteadyNow() - max_age;

		while (auto chunk = LockPopQueue()) {
			if (chunk->time < min_time)
				/* discard old chunks */
				continue;

			const std::span payload = chunk->payload;
			if (!SendWireChunk(payload, chunk->time)) {
				// TODO: handle EAGAIN
				LockClose();
				return;
			}
		}

		event.CancelWrite();
	}

	BufferedSocket::OnSocketReady(flags);
}

static bool
Send(SocketDescriptor s, std::span<const std::byte> buffer) noexcept
{
	auto nbytes = s.Send(buffer);
	return nbytes == ssize_t(buffer.size());
}

template<typename T>
static bool
SendT(SocketDescriptor s, const T &buffer) noexcept
{
	return Send(s, ReferenceAsBytes(buffer));
}

static bool
Send(SocketDescriptor s, std::string_view buffer) noexcept
{
	return Send(s, AsBytes(buffer));
}

static bool
SendServerSettings(SocketDescriptor s, const PackedBE16 id,
		   const SnapcastBase &request,
		   const std::string_view payload) noexcept
{
	const PackedLE32 payload_size = payload.size();

	SnapcastBase base{};
	base.type = uint16_t(SnapcastMessageType::SERVER_SETTINGS);
	base.id = id;
	base.refers_to = request.id;
	base.sent = ToSnapcastTimestamp(std::chrono::steady_clock::now());
	base.size = sizeof(payload_size) + payload.size();

	return SendT(s, base) && SendT(s, payload_size) && Send(s, payload);
}

bool
SnapcastClient::SendServerSettings(const SnapcastBase &request) noexcept
{
	// TODO: make settings configurable
	return ::SendServerSettings(GetSocket(), next_id++, request,
				    R"({"bufferMs": 1000})");
}

static bool
SendCodecHeader(SocketDescriptor s, const PackedBE16 id,
		const SnapcastBase &request,
		const std::string_view codec,
		const std::span<const std::byte> payload) noexcept
{
	const PackedLE32 codec_size = codec.size();
	const PackedLE32 payload_size = payload.size();

	SnapcastBase base{};
	base.type = uint16_t(SnapcastMessageType::CODEC_HEADER);
	base.id = id;
	base.refers_to = request.id;
	base.sent = ToSnapcastTimestamp(std::chrono::steady_clock::now());
	base.size = sizeof(codec_size) + codec.size() +
		sizeof(payload_size) + payload.size();

	return SendT(s, base) &&
		SendT(s, codec_size) && Send(s, codec) &&
		SendT(s, payload_size) && Send(s, payload);
}

bool
SnapcastClient::SendCodecHeader(const SnapcastBase &request) noexcept
{
	return ::SendCodecHeader(GetSocket(), next_id++, request,
				 output.GetCodecName(),
				 output.GetCodecHeader());
}

static bool
SendTime(SocketDescriptor s, const PackedBE16 id,
	 const SnapcastBase &request_header,
	 const SnapcastTime &request_payload) noexcept
{
	SnapcastTime payload = request_payload;
	payload.latency = request_header.received - request_header.sent;

	SnapcastBase base{};
	base.type = uint16_t(SnapcastMessageType::TIME);
	base.id = id;
	base.refers_to = request_header.id;
	base.sent = ToSnapcastTimestamp(std::chrono::steady_clock::now());
	base.size = sizeof(payload);

	return SendT(s, base) && SendT(s, payload);
}

bool
SnapcastClient::SendTime(const SnapcastBase &request_header,
			 const SnapcastTime &request_payload) noexcept
{
	return ::SendTime(GetSocket(), next_id++,
			  request_header, request_payload);
}

static bool
SendWireChunk(SocketDescriptor s, const PackedBE16 id,
	      const std::span<const std::byte> payload,
	      std::chrono::steady_clock::time_point t) noexcept
{
	SnapcastWireChunk hdr{};
	hdr.timestamp = ToSnapcastTimestamp(t);
	hdr.size = payload.size();

	SnapcastBase base{};
	base.type = uint16_t(SnapcastMessageType::WIRE_CHUNK);
	base.id = id;
	base.sent = ToSnapcastTimestamp(std::chrono::steady_clock::now());
	base.size = sizeof(hdr) + payload.size();

	// TODO: no blocking send()
	return SendT(s, base) && SendT(s, hdr) && Send(s, payload);
}

bool
SnapcastClient::SendWireChunk(std::span<const std::byte> payload,
			      std::chrono::steady_clock::time_point t) noexcept
{
	return ::SendWireChunk(GetSocket(), next_id++, payload, t);
}

static bool
SendStreamTags(SocketDescriptor s, const PackedBE16 id,
	       const std::span<const std::byte> payload) noexcept
{
	const PackedLE32 payload_size = payload.size();

	SnapcastBase base{};
	base.type = uint16_t(SnapcastMessageType::STREAM_TAGS);
	base.id = id;
	base.sent = ToSnapcastTimestamp(std::chrono::steady_clock::now());
	base.size = sizeof(payload_size) + payload.size();

	return SendT(s, base) && SendT(s, payload_size) && Send(s, payload);
}

void
SnapcastClient::SendStreamTags(std::span<const std::byte> payload) noexcept
{
	::SendStreamTags(GetSocket(), next_id++, payload);
}

BufferedSocket::InputResult
SnapcastClient::OnSocketInput(std::span<std::byte> src) noexcept
{
	auto &base = *(SnapcastBase *)src.data();

	if (src.size() < sizeof(base) ||
	    src.size() < sizeof(base) + base.size)
		return InputResult::MORE;

	base.received = ToSnapcastTimestamp(GetEventLoop().SteadyNow());

	ConsumeInput(sizeof(base) + base.size);

	const std::span<const std::byte> payload{(const std::byte *)(&base + 1), base.size};

	switch (SnapcastMessageType(uint16_t(base.type))) {
	case SnapcastMessageType::HELLO:
		if (!SendServerSettings(base) ||
		    !SendCodecHeader(base)) {
			LockClose();
			return InputResult::CLOSED;
		}

		active = true;
		break;

	case SnapcastMessageType::TIME:
		if (payload.size() >= sizeof(SnapcastTime))
			SendTime(base, *(const SnapcastTime *)(const void *)payload.data());
		break;

	default:
		LockClose();
		return InputResult::CLOSED;
	}

	return InputResult::AGAIN;
}

void
SnapcastClient::OnSocketError(std::exception_ptr ep) noexcept
{
	LogError(ep);
	LockClose();
}

void
SnapcastClient::OnSocketClosed() noexcept
{
	LockClose();
}
