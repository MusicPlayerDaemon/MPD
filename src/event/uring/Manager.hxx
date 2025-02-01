// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "io/uring/Queue.hxx"

namespace Uring {

class Manager final : public Queue {
public:
	using Queue::Queue;

	// virtual methods from class Uring::Queue
	void Submit() override {
		/* this will be done by EventLoop::Run() */
	}
};

} // namespace Uring
