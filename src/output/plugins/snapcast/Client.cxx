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

#include "Client.hxx"
#include "Protocol.hxx"
#include "Timestamp.hxx"
#include "Internal.hxx"
#include "tag/RiffFormat.hxx"
#include "event/Loop.hxx"
#include "net/SocketError.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "util/StringView.hxx"
#include "Log.hxx"

#include <cassert>
#include <cstring>

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
	const std::scoped_lock<Mutex> protect(output.mutex);
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
	const std::scoped_lock<Mutex> protect(output.mutex);
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

			const ConstBuffer<std::byte> payload = chunk->payload;
			if (!SendWireChunk(payload.ToVoid(), chunk->time)) {
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
Send(SocketDescriptor s, ConstBuffer<void> buffer) noexcept
{
	auto nbytes = s.Write(buffer.data, buffer.size);
	return nbytes == ssize_t(buffer.size);
}

template<typename T>
static bool
SendT(SocketDescriptor s, const T &buffer) noexcept
{
	return Send(s, ConstBuffer<T>{&buffer, 1}.ToVoid());
}

static bool
Send(SocketDescriptor s, StringView buffer) noexcept
{
	return Send(s, buffer.ToVoid());
}

static bool
SendServerSettings(SocketDescriptor s, const PackedBE16 id,
		   const SnapcastBase &request,
		   const StringView payload) noexcept
{
	const PackedLE32 payload_size = payload.size;

	SnapcastBase base{};
	base.type = uint16_t(SnapcastMessageType::SERVER_SETTINGS);
	base.id = id;
	base.refers_to = request.id;
	base.sent = ToSnapcastTimestamp(std::chrono::steady_clock::now());
	base.size = sizeof(payload_size) + payload.size;

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
		const StringView codec,
		const ConstBuffer<void> payload) noexcept
{
	const PackedLE32 codec_size = codec.size;
	const PackedLE32 payload_size = payload.size;

	SnapcastBase base{};
	base.type = uint16_t(SnapcastMessageType::CODEC_HEADER);
	base.id = id;
	base.refers_to = request.id;
	base.sent = ToSnapcastTimestamp(std::chrono::steady_clock::now());
	base.size = sizeof(codec_size) + codec.size +
		sizeof(payload_size) + payload.size;

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
	      const ConstBuffer<void> payload,
	      std::chrono::steady_clock::time_point t) noexcept
{
	SnapcastWireChunk hdr{};
	hdr.timestamp = ToSnapcastTimestamp(t);
	hdr.size = payload.size;

	SnapcastBase base{};
	base.type = uint16_t(SnapcastMessageType::WIRE_CHUNK);
	base.id = id;
	base.sent = ToSnapcastTimestamp(std::chrono::steady_clock::now());
	base.size = sizeof(hdr) + payload.size;

	// TODO: no blocking send()
	return SendT(s, base) && SendT(s, hdr) && Send(s, payload);
}

bool
SnapcastClient::SendWireChunk(ConstBuffer<void> payload,
			      std::chrono::steady_clock::time_point t) noexcept
{
	return ::SendWireChunk(GetSocket(), next_id++, payload, t);
}

static bool
SendStreamTags(SocketDescriptor s, const PackedBE16 id,
	       const ConstBuffer<void> payload) noexcept
{
	const PackedLE32 payload_size = payload.size;

	SnapcastBase base{};
	base.type = uint16_t(SnapcastMessageType::STREAM_TAGS);
	base.id = id;
	base.sent = ToSnapcastTimestamp(std::chrono::steady_clock::now());
	base.size = sizeof(payload_size) + payload.size;

	return SendT(s, base) && SendT(s, payload_size) && Send(s, payload);
}

void
SnapcastClient::SendStreamTags(ConstBuffer<void> payload) noexcept
{
	::SendStreamTags(GetSocket(), next_id++, payload);
}

BufferedSocket::InputResult
SnapcastClient::OnSocketInput(void *data, size_t length) noexcept
{
	auto &base = *(SnapcastBase *)data;

	if (length < sizeof(base) ||
	    length < sizeof(base) + base.size)
		return InputResult::MORE;

	base.received = ToSnapcastTimestamp(GetEventLoop().SteadyNow());

	ConsumeInput(sizeof(base) + base.size);

	const ConstBuffer<void> payload{&base + 1, base.size};

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
		if (payload.size >= sizeof(SnapcastTime))
			SendTime(base, *(const SnapcastTime *)payload.data);
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
