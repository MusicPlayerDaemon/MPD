// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include <stdexcept>

class SocketProtocolError : public std::runtime_error {
public:
	using std::runtime_error::runtime_error;
};

class SocketMessageTooLargeError : public SocketProtocolError {
public:
	using SocketProtocolError::SocketProtocolError;
};

class SocketGarbageReceivedError : public SocketProtocolError {
public:
	using SocketProtocolError::SocketProtocolError;
};

class SocketClosedPrematurelyError : public SocketProtocolError {
public:
	using SocketProtocolError::SocketProtocolError;

	SocketClosedPrematurelyError() noexcept
		:SocketProtocolError("Peer closed the socket prematurely") {}
};

class SocketBufferFullError : public SocketProtocolError {
public:
	using SocketProtocolError::SocketProtocolError;

	SocketBufferFullError() noexcept
		:SocketProtocolError("Socket buffer overflow") {}
};
