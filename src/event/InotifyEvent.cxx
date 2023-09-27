// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "InotifyEvent.hxx"
#include "lib/fmt/SystemError.hxx"
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
		throw FmtErrno("inotify_add_watch('{}') failed", pathname);

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

	ssize_t nbytes = event.GetFileDescriptor().Read(buffer);
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
