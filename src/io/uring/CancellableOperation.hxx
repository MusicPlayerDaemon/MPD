/*
 * Copyright 2020 CM4all GmbH
 * All rights reserved.
 *
 * author: Max Kellermann <mk@cm4all.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "Operation.hxx"
#include "util/IntrusiveList.hxx"

#include <cassert>
#include <utility>

namespace Uring {

class CancellableOperation : public IntrusiveListHook
{
	Operation *operation;

public:
	CancellableOperation(Operation &_operation) noexcept
		:operation(&_operation)
	{
		assert(operation->cancellable == nullptr);
		operation->cancellable = this;
	}

	~CancellableOperation() noexcept {
		assert(operation == nullptr);
	}

	void Cancel(Operation &_operation) noexcept {
		(void)_operation;
		assert(operation == &_operation);

		operation = nullptr;

		// TODO: io_uring_prep_cancel()
	}

	void Replace(Operation &old_operation,
		     Operation &new_operation) noexcept {
		assert(operation == &old_operation);
		assert(old_operation.cancellable == this);

		old_operation.cancellable = nullptr;
		operation = &new_operation;
		new_operation.cancellable = this;
	}

	void OnUringCompletion(int res) noexcept {
		if (operation == nullptr)
			return;

		assert(operation->cancellable == this);
		operation->cancellable = nullptr;

		std::exchange(operation, nullptr)->OnUringCompletion(res);
	}
};

} // namespace Uring
