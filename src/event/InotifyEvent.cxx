/*
 * Copyright 2022 CM4all GmbH
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

#include "InotifyEvent.hxx"
#include "system/Error.hxx"
#include "io/UniqueFileDescriptor.hxx"

#include <array>

#include <limits.h>
#include <sys/inotify.h>

static UniqueFileDescriptor
CreateInotify()
{
	int fd = inotify_init1(IN_CLOEXEC|IN_NONBLOCK);
	if (fd < 0)
		throw MakeErrno("inotify_init1() failed");

	return UniqueFileDescriptor(fd);
}

InotifyEvent::InotifyEvent(EventLoop &event_loop, InotifyHandler &_handler)
	:event(event_loop, BIND_THIS_METHOD(OnInotifyReady),
	       CreateInotify().Release()),
	 handler(_handler)
{
	Enable();
}

InotifyEvent::~InotifyEvent() noexcept
{
	Close();
}

int
InotifyEvent::AddWatch(const char *pathname, uint32_t mask)
{
	int wd = inotify_add_watch(event.GetFileDescriptor().Get(),
				   pathname, mask);
	if (wd < 0)
		throw FormatErrno("inotify_add_watch('%s') failed", pathname);

	return wd;
}

int
InotifyEvent::AddModifyWatch(const char *pathname)
{
	return AddWatch(pathname, IN_MODIFY);
}

void
InotifyEvent::RemoveWatch(int wd) noexcept
{
	inotify_rm_watch(event.GetFileDescriptor().Get(), wd);
}

inline void
InotifyEvent::OnInotifyReady(unsigned) noexcept
try {
	std::array<std::byte, 4096> buffer;
	static_assert(sizeof(buffer) >= sizeof(struct inotify_event) + NAME_MAX + 1,
		      "inotify buffer too small");

	ssize_t nbytes = event.GetFileDescriptor().Read(buffer.data(),
							buffer.size());
	if (nbytes <= 0) [[unlikely]] {
		if (nbytes == 0)
			throw std::runtime_error{"EOF from inotify"};

		const int e = errno;
		if (e == EAGAIN)
			return;

		throw MakeErrno(e, "Reading inotify failed");
	}

	const std::byte *p = buffer.data(), *const end = p + nbytes;

	while (true) {
		const size_t remaining = end - p;
		const auto &ie = *(const struct inotify_event *)(const void *)p;
		if (remaining < sizeof(ie) ||
		    remaining < sizeof(ie) + ie.len)
			break;

		const char *name;
		if (ie.len > 0 && ie.name[ie.len - 1] == 0)
			name = ie.name;
		else
			name = nullptr;

		handler.OnInotify(ie.wd, ie.mask, name);
		p += sizeof(ie) + ie.len;
	}
} catch (...) {
	Close();
	handler.OnInotifyError(std::current_exception());
}
