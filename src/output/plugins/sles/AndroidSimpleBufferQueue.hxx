/*
 * Copyright (C) 2011-2012 Max Kellermann <max.kellermann@gmail.com>
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

#ifndef SLES_ANDROID_SIMPLE_BUFFER_QUEUE_HPP
#define SLES_ANDROID_SIMPLE_BUFFER_QUEUE_HPP

#include <SLES/OpenSLES_Android.h>

namespace SLES {
	/**
	 * OO wrapper for an OpenSL/ES SLAndroidSimpleBufferQueueItf
	 * variable.
	 */
	class AndroidSimpleBufferQueue {
		SLAndroidSimpleBufferQueueItf queue;

	public:
		AndroidSimpleBufferQueue() = default;
		explicit AndroidSimpleBufferQueue(SLAndroidSimpleBufferQueueItf _queue)
			:queue(_queue) {}

		SLresult Enqueue(const void *pBuffer, SLuint32 size) {
			return (*queue)->Enqueue(queue, pBuffer, size);
		}

		SLresult Clear() {
			return (*queue)->Clear(queue);
		}

		SLresult GetState(SLAndroidSimpleBufferQueueState *pState) {
			return (*queue)->GetState(queue, pState);
		}

		SLresult RegisterCallback(slAndroidSimpleBufferQueueCallback callback,
					  void *pContext) {
			return (*queue)->RegisterCallback(queue, callback, pContext);
		}
	};
}

#endif
