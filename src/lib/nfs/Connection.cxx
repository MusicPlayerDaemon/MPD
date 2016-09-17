/*
 * Copyright 2003-2016 The Music Player Daemon Project
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

#include "config.h"
#include "Connection.hxx"
#include "Lease.hxx"
#include "Callback.hxx"
#include "event/Loop.hxx"
#include "system/fd_util.h"
#include "util/RuntimeError.hxx"

extern "C" {
#include <nfsc/libnfs.h>
}

#include <utility>

#include <poll.h> /* for POLLIN, POLLOUT */

static constexpr unsigned NFS_MOUNT_TIMEOUT = 60;

inline void
NfsConnection::CancellableCallback::Stat(nfs_context *ctx,
					 const char *path)
{
	assert(connection.GetEventLoop().IsInside());

	int result = nfs_stat_async(ctx, path, Callback, this);
	if (result < 0)
		throw FormatRuntimeError("nfs_stat_async() failed: %s",
					 nfs_get_error(ctx));
}

inline void
NfsConnection::CancellableCallback::OpenDirectory(nfs_context *ctx,
						  const char *path)
{
	assert(connection.GetEventLoop().IsInside());

	int result = nfs_opendir_async(ctx, path, Callback, this);
	if (result < 0)
		throw FormatRuntimeError("nfs_opendir_async() failed: %s",
					 nfs_get_error(ctx));
}

inline void
NfsConnection::CancellableCallback::Open(nfs_context *ctx,
					 const char *path, int flags)
{
	assert(connection.GetEventLoop().IsInside());

	int result = nfs_open_async(ctx, path, flags,
				    Callback, this);
	if (result < 0)
		throw FormatRuntimeError("nfs_open_async() failed: %s",
					 nfs_get_error(ctx));
}

inline void
NfsConnection::CancellableCallback::Stat(nfs_context *ctx,
					 struct nfsfh *fh)
{
	assert(connection.GetEventLoop().IsInside());

	int result = nfs_fstat_async(ctx, fh, Callback, this);
	if (result < 0)
		throw FormatRuntimeError("nfs_fstat_async() failed: %s",
					 nfs_get_error(ctx));
}

inline void
NfsConnection::CancellableCallback::Read(nfs_context *ctx, struct nfsfh *fh,
					 uint64_t offset, size_t size)
{
	assert(connection.GetEventLoop().IsInside());

	int result = nfs_pread_async(ctx, fh, offset, size, Callback, this);
	if (result < 0)
		throw FormatRuntimeError("nfs_pread_async() failed: %s",
					 nfs_get_error(ctx));
}

inline void
NfsConnection::CancellableCallback::CancelAndScheduleClose(struct nfsfh *fh)
{
	assert(connection.GetEventLoop().IsInside());
	assert(!open);
	assert(close_fh == nullptr);
	assert(fh != nullptr);

	close_fh = fh;
	Cancel();
}

inline void
NfsConnection::CancellableCallback::PrepareDestroyContext()
{
	assert(IsCancelled());

	if (close_fh != nullptr) {
		connection.InternalClose(close_fh);
		close_fh = nullptr;
	}
}

inline void
NfsConnection::CancellableCallback::Callback(int err, void *data)
{
	assert(connection.GetEventLoop().IsInside());

	if (!IsCancelled()) {
		assert(close_fh == nullptr);

		NfsCallback &cb = Get();

		connection.callbacks.Remove(*this);

		if (err >= 0)
			cb.OnNfsCallback((unsigned)err, data);
		else
			cb.OnNfsError(std::make_exception_ptr(std::runtime_error((const char *)data)));
	} else {
		if (open) {
			/* a nfs_open_async() call was cancelled - to
			   avoid a memory leak, close the newly
			   allocated file handle immediately */
			assert(close_fh == nullptr);

			if (err >= 0) {
				struct nfsfh *fh = (struct nfsfh *)data;
				connection.Close(fh);
			}
		} else if (close_fh != nullptr)
			connection.DeferClose(close_fh);

		connection.callbacks.Remove(*this);
	}
}

void
NfsConnection::CancellableCallback::Callback(int err,
					     gcc_unused struct nfs_context *nfs,
					     void *data, void *private_data)
{
	CancellableCallback &c = *(CancellableCallback *)private_data;
	c.Callback(err, data);
}

static constexpr unsigned
libnfs_to_events(int i)
{
	return ((i & POLLIN) ? SocketMonitor::READ : 0) |
		((i & POLLOUT) ? SocketMonitor::WRITE : 0);
}

static constexpr int
events_to_libnfs(unsigned i)
{
	return ((i & SocketMonitor::READ) ? POLLIN : 0) |
		((i & SocketMonitor::WRITE) ? POLLOUT : 0);
}

NfsConnection::~NfsConnection()
{
	assert(GetEventLoop().IsInside());
	assert(new_leases.empty());
	assert(active_leases.empty());
	assert(callbacks.IsEmpty());
	assert(deferred_close.empty());

	if (context != nullptr)
		DestroyContext();
}

void
NfsConnection::AddLease(NfsLease &lease)
{
	assert(GetEventLoop().IsInside());

	new_leases.push_back(&lease);

	DeferredMonitor::Schedule();
}

void
NfsConnection::RemoveLease(NfsLease &lease)
{
	assert(GetEventLoop().IsInside());

	new_leases.remove(&lease);
	active_leases.remove(&lease);
}

void
NfsConnection::Stat(const char *path, NfsCallback &callback)
{
	assert(GetEventLoop().IsInside());
	assert(!callbacks.Contains(callback));

	auto &c = callbacks.Add(callback, *this, false);
	try {
		c.Stat(context, path);
	} catch (...) {
		callbacks.Remove(c);
		throw;
	}

	ScheduleSocket();
}

void
NfsConnection::OpenDirectory(const char *path, NfsCallback &callback)
{
	assert(GetEventLoop().IsInside());
	assert(!callbacks.Contains(callback));

	auto &c = callbacks.Add(callback, *this, true);
	try {
		c.OpenDirectory(context, path);
	} catch (...) {
		callbacks.Remove(c);
		throw;
	}

	ScheduleSocket();
}

const struct nfsdirent *
NfsConnection::ReadDirectory(struct nfsdir *dir)
{
	assert(GetEventLoop().IsInside());

	return nfs_readdir(context, dir);
}

void
NfsConnection::CloseDirectory(struct nfsdir *dir)
{
	assert(GetEventLoop().IsInside());

	return nfs_closedir(context, dir);
}

void
NfsConnection::Open(const char *path, int flags, NfsCallback &callback)
{
	assert(GetEventLoop().IsInside());
	assert(!callbacks.Contains(callback));

	auto &c = callbacks.Add(callback, *this, true);
	try {
		c.Open(context, path, flags);
	} catch (...) {
		callbacks.Remove(c);
		throw;
	}

	ScheduleSocket();
}

void
NfsConnection::Stat(struct nfsfh *fh, NfsCallback &callback)
{
	assert(GetEventLoop().IsInside());
	assert(!callbacks.Contains(callback));

	auto &c = callbacks.Add(callback, *this, false);
	try {
		c.Stat(context, fh);
	} catch (...) {
		callbacks.Remove(c);
		throw;
	}

	ScheduleSocket();
}

void
NfsConnection::Read(struct nfsfh *fh, uint64_t offset, size_t size,
		    NfsCallback &callback)
{
	assert(GetEventLoop().IsInside());
	assert(!callbacks.Contains(callback));

	auto &c = callbacks.Add(callback, *this, false);
	try {
		c.Read(context, fh, offset, size);
	} catch (...) {
		callbacks.Remove(c);
		throw;
	}

	ScheduleSocket();
}

void
NfsConnection::Cancel(NfsCallback &callback)
{
	callbacks.Cancel(callback);
}

static void
DummyCallback(int, struct nfs_context *, void *, void *)
{
}

inline void
NfsConnection::InternalClose(struct nfsfh *fh)
{
	assert(GetEventLoop().IsInside());
	assert(context != nullptr);
	assert(fh != nullptr);

	nfs_close_async(context, fh, DummyCallback, nullptr);
}

void
NfsConnection::Close(struct nfsfh *fh)
{
	assert(GetEventLoop().IsInside());

	InternalClose(fh);
	ScheduleSocket();
}

void
NfsConnection::CancelAndClose(struct nfsfh *fh, NfsCallback &callback)
{
	CancellableCallback &cancel = callbacks.Get(callback);
	cancel.CancelAndScheduleClose(fh);
}

void
NfsConnection::DestroyContext()
{
	assert(GetEventLoop().IsInside());
	assert(context != nullptr);

#ifndef NDEBUG
	assert(!in_destroy);
	in_destroy = true;
#endif

	if (!mount_finished) {
		assert(TimeoutMonitor::IsActive());
		TimeoutMonitor::Cancel();
	}

	/* cancel pending DeferredMonitor that was scheduled to notify
	   new leases */
	DeferredMonitor::Cancel();

	if (SocketMonitor::IsDefined())
		SocketMonitor::Steal();

	callbacks.ForEach([](CancellableCallback &c){
			c.PrepareDestroyContext();
		});

	nfs_destroy_context(context);
	context = nullptr;
}

inline void
NfsConnection::DeferClose(struct nfsfh *fh)
{
	assert(GetEventLoop().IsInside());
	assert(in_event);
	assert(in_service);
	assert(context != nullptr);
	assert(fh != nullptr);

	deferred_close.push_front(fh);
}

void
NfsConnection::ScheduleSocket()
{
	assert(GetEventLoop().IsInside());
	assert(context != nullptr);

	if (!SocketMonitor::IsDefined()) {
		int _fd = nfs_get_fd(context);
		if (_fd < 0)
			return;

		fd_set_cloexec(_fd, true);
		SocketMonitor::Open(_fd);
	}

	SocketMonitor::Schedule(libnfs_to_events(nfs_which_events(context)));
}

inline int
NfsConnection::Service(unsigned flags)
{
	assert(GetEventLoop().IsInside());
	assert(context != nullptr);

#ifndef NDEBUG
	assert(!in_event);
	in_event = true;

	assert(!in_service);
	in_service = true;
#endif

	int result = nfs_service(context, events_to_libnfs(flags));

#ifndef NDEBUG
	assert(context != nullptr);
	assert(in_service);
	in_service = false;
#endif

	return result;
}

bool
NfsConnection::OnSocketReady(unsigned flags)
{
	assert(GetEventLoop().IsInside());
	assert(deferred_close.empty());

	bool closed = false;

	const bool was_mounted = mount_finished;
	if (!mount_finished)
		/* until the mount is finished, the NFS client may use
		   various sockets, therefore we unregister and
		   re-register it each time */
		SocketMonitor::Steal();

	const int result = Service(flags);

	while (!deferred_close.empty()) {
		InternalClose(deferred_close.front());
		deferred_close.pop_front();
	}

	if (!was_mounted && mount_finished) {
		if (postponed_mount_error) {
			DestroyContext();
			closed = true;
			BroadcastMountError(std::move(postponed_mount_error));
		} else if (result == 0)
			BroadcastMountSuccess();
	} else if (result < 0) {
		/* the connection has failed */

		auto e = FormatRuntimeError("NFS connection has failed: %s",
					    nfs_get_error(context));
		BroadcastError(std::make_exception_ptr(e));

		DestroyContext();
		closed = true;
	} else if (nfs_get_fd(context) < 0) {
		/* this happens when rpc_reconnect_requeue() is called
		   after the connection broke, but autoreconnect was
		   disabled - nfs_service() returns 0 */

		const char *msg = nfs_get_error(context);
		if (msg == nullptr)
			msg = "<unknown>";
		auto e = FormatRuntimeError("NFS socket disappeared: %s", msg);

		BroadcastError(std::make_exception_ptr(e));

		DestroyContext();
		closed = true;
	}

	assert(context == nullptr || nfs_get_fd(context) >= 0);

#ifndef NDEBUG
	assert(in_event);
	in_event = false;
#endif

	if (context != nullptr)
		ScheduleSocket();

	return !closed;
}

inline void
NfsConnection::MountCallback(int status, gcc_unused nfs_context *nfs,
			     gcc_unused void *data)
{
	assert(GetEventLoop().IsInside());
	assert(context == nfs);

	mount_finished = true;

	assert(TimeoutMonitor::IsActive() || in_destroy);
	TimeoutMonitor::Cancel();

	if (status < 0) {
		auto e = FormatRuntimeError("nfs_mount_async() failed: %s",
					    nfs_get_error(context));
		postponed_mount_error = std::make_exception_ptr(e);
		return;
	}
}

void
NfsConnection::MountCallback(int status, nfs_context *nfs, void *data,
			     void *private_data)
{
	NfsConnection *c = (NfsConnection *)private_data;

	c->MountCallback(status, nfs, data);
}

inline void
NfsConnection::MountInternal()
{
	assert(GetEventLoop().IsInside());
	assert(context == nullptr);

	context = nfs_init_context();
	if (context == nullptr)
		throw std::runtime_error("nfs_init_context() failed");

	postponed_mount_error = std::exception_ptr();
	mount_finished = false;

	TimeoutMonitor::ScheduleSeconds(NFS_MOUNT_TIMEOUT);

#ifndef NDEBUG
	in_service = false;
	in_event = false;
	in_destroy = false;
#endif

	if (nfs_mount_async(context, server.c_str(), export_name.c_str(),
			    MountCallback, this) != 0) {
		auto e = FormatRuntimeError("nfs_mount_async() failed: %s",
					    nfs_get_error(context));
		nfs_destroy_context(context);
		context = nullptr;
		throw e;
	}

	ScheduleSocket();
}

void
NfsConnection::BroadcastMountSuccess()
{
	assert(GetEventLoop().IsInside());

	while (!new_leases.empty()) {
		auto i = new_leases.begin();
		active_leases.splice(active_leases.end(), new_leases, i);
		(*i)->OnNfsConnectionReady();
	}
}

void
NfsConnection::BroadcastMountError(std::exception_ptr &&e)
{
	assert(GetEventLoop().IsInside());

	while (!new_leases.empty()) {
		auto l = new_leases.front();
		new_leases.pop_front();
		l->OnNfsConnectionFailed(e);
	}

	OnNfsConnectionError(std::move(e));
}

void
NfsConnection::BroadcastError(std::exception_ptr &&e)
{
	assert(GetEventLoop().IsInside());

	while (!active_leases.empty()) {
		auto l = active_leases.front();
		active_leases.pop_front();
		l->OnNfsConnectionDisconnected(e);
	}

	BroadcastMountError(std::move(e));
}

void
NfsConnection::OnTimeout()
{
	assert(GetEventLoop().IsInside());
	assert(!mount_finished);

	mount_finished = true;
	DestroyContext();

	BroadcastMountError(std::make_exception_ptr(std::runtime_error("Mount timeout")));
}

void
NfsConnection::RunDeferred()
{
	assert(GetEventLoop().IsInside());

	if (context == nullptr) {
		try {
			MountInternal();
		} catch (...) {
			BroadcastMountError(std::current_exception());
			return;
		}
	}

	if (mount_finished)
		BroadcastMountSuccess();
}
