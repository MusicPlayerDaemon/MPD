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

namespace Uring {

class CancellableOperation;

/**
 * An asynchronous I/O operation to be queued in a #Queue instance.
 */
class Operation {
	friend class CancellableOperation;

	CancellableOperation *cancellable = nullptr;

public:
	Operation() noexcept = default;

	~Operation() noexcept {
		CancelUring();
	}

	Operation(const Operation &) = delete;
	Operation &operator=(const Operation &) = delete;

	/**
	 * Are we waiting for the operation to complete?
	 */
	bool IsUringPending() const noexcept {
		return cancellable != nullptr;
	}

	/**
	 * Cancel the operation.  OnUringCompletion() will not be
	 * invoked.  This is a no-op if none is pending.
	 */
	void CancelUring() noexcept;

	/**
	 * Replace this pending operation with a new one.  This method
	 * is only legal if IsUringPending().
	 */
	void ReplaceUring(Operation &new_operation) noexcept;

	/**
	 * This method is called when the operation completes.
	 *
	 * @param res the result code; the meaning is specific to the
	 * operation, but negative values usually mean an error has
	 * occurred
	 */
	virtual void OnUringCompletion(int res) noexcept = 0;
};

} // namespace Uring
