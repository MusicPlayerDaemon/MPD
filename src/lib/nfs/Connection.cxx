/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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
#include "Domain.hxx"
#include "Callback.hxx"
#include "system/fd_util.h"
#include "util/Error.hxx"
#include "event/Call.hxx"

extern "C" {
#include <nfsc/libnfs.h>
}

#include <utility>

inline bool
NfsConnection::CancellableCallback::Open(nfs_context *ctx,
					 const char *path, int flags,
					 Error &error)
{
	int result = nfs_open_async(ctx, path, flags,
				    Callback, this);
	if (result < 0) {
		error.Format(nfs_domain, "nfs_open_async() failed: %s",
			     nfs_get_error(ctx));
		return false;
	}

	return true;
}

inline bool
NfsConnection::CancellableCallback::Stat(nfs_context *ctx,
					 struct nfsfh *fh,
					 Error &error)
{
	int result = nfs_fstat_async(ctx, fh, Callback, this);
	if (result < 0) {
		error.Format(nfs_domain, "nfs_fstat_async() failed: %s",
			     nfs_get_error(ctx));
		return false;
	}

	return true;
}

inline bool
NfsConnection::CancellableCallback::Read(nfs_context *ctx, struct nfsfh *fh,
					 uint64_t offset, size_t size,
					 Error &error)
{
	int result = nfs_pread_async(ctx, fh, offset, size, Callback, this);
	if (result < 0) {
		error.Format(nfs_domain, "nfs_pread_async() failed: %s",
			     nfs_get_error(ctx));
		return false;
	}

	return true;
}

inline void
NfsConnection::CancellableCallback::Callback(int err, void *data)
{
	if (!IsCancelled()) {
		NfsCallback &cb = Get();

		connection.callbacks.Remove(*this);

		if (err >= 0)
			cb.OnNfsCallback((unsigned)err, data);
		else
			cb.OnNfsError(Error(nfs_domain, err,
					    (const char *)data));
	} else {
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
	assert(new_leases.empty());
	assert(active_leases.empty());
	assert(callbacks.IsEmpty());

	if (context != nullptr)
		BlockingCall(SocketMonitor::GetEventLoop(), [this](){
				DestroyContext();
			});
}

void
NfsConnection::AddLease(NfsLease &lease)
{
	{
		const ScopeLock protect(mutex);
		new_leases.push_back(&lease);
	}

	DeferredMonitor::Schedule();
}

void
NfsConnection::RemoveLease(NfsLease &lease)
{
	const ScopeLock protect(mutex);

	new_leases.remove(&lease);
	active_leases.remove(&lease);
}

bool
NfsConnection::Open(const char *path, int flags, NfsCallback &callback,
		    Error &error)
{
	assert(!callbacks.Contains(callback));

	auto &c = callbacks.Add(callback, *this);
	if (!c.Open(context, path, flags, error)) {
		callbacks.RemoveLast();
		return false;
	}

	ScheduleSocket();
	return true;
}

bool
NfsConnection::Stat(struct nfsfh *fh, NfsCallback &callback, Error &error)
{
	assert(!callbacks.Contains(callback));

	auto &c = callbacks.Add(callback, *this);
	if (!c.Stat(context, fh, error)) {
		callbacks.RemoveLast();
		return false;
	}

	ScheduleSocket();
	return true;
}

bool
NfsConnection::Read(struct nfsfh *fh, uint64_t offset, size_t size,
		    NfsCallback &callback, Error &error)
{
	assert(!callbacks.Contains(callback));

	auto &c = callbacks.Add(callback, *this);
	if (!c.Read(context, fh, offset, size, error)) {
		callbacks.RemoveLast();
		return false;
	}

	ScheduleSocket();
	return true;
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

void
NfsConnection::Close(struct nfsfh *fh)
{
	nfs_close_async(context, fh, DummyCallback, nullptr);
	ScheduleSocket();
}

void
NfsConnection::DestroyContext()
{
	assert(context != nullptr);

	SocketMonitor::Cancel();
	nfs_destroy_context(context);
	context = nullptr;
}

void
NfsConnection::ScheduleSocket()
{
	assert(context != nullptr);

	if (!SocketMonitor::IsDefined()) {
		int _fd = nfs_get_fd(context);
		fd_set_cloexec(_fd, true);
		SocketMonitor::Open(_fd);
	}

	SocketMonitor::Schedule(libnfs_to_events(nfs_which_events(context)));
}

bool
NfsConnection::OnSocketReady(unsigned flags)
{
	bool closed = false;

	const bool was_mounted = mount_finished;
	if (!mount_finished)
		/* until the mount is finished, the NFS client may use
		   various sockets, therefore we unregister and
		   re-register it each time */
		SocketMonitor::Steal();

	assert(!in_event);
	in_event = true;

	assert(!in_service);
	in_service = true;
	postponed_destroy = false;

	int result = nfs_service(context, events_to_libnfs(flags));

	assert(context != nullptr);
	assert(in_service);
	in_service = false;

	if (postponed_destroy) {
		/* somebody has called nfs_client_free() while we were inside
		   nfs_service() */
		const ScopeLock protect(mutex);
		DestroyContext();
		closed = true;
		// TODO? nfs_client_cleanup_files(client);
	} else if (!was_mounted && mount_finished) {
		const ScopeLock protect(mutex);

		if (postponed_mount_error.IsDefined()) {
			DestroyContext();
			closed = true;
			BroadcastMountError(std::move(postponed_mount_error));
		} else if (result == 0)
			BroadcastMountSuccess();
	} else if (result < 0) {
		/* the connection has failed */
		Error error;
		error.Format(nfs_domain, "NFS connection has failed: %s",
			     nfs_get_error(context));

		const ScopeLock protect(mutex);

		DestroyContext();
		closed = true;

		if (!mount_finished)
			BroadcastMountError(std::move(error));
		else
			BroadcastError(std::move(error));
	}

	assert(in_event);
	in_event = false;

	if (context != nullptr)
		ScheduleSocket();

	return !closed;
}

inline void
NfsConnection::MountCallback(int status, gcc_unused nfs_context *nfs,
			     gcc_unused void *data)
{
	assert(context == nfs);

	mount_finished = true;

	if (status < 0) {
		postponed_mount_error.Set(nfs_domain, status,
					  "nfs_mount_async() failed");
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

inline bool
NfsConnection::MountInternal(Error &error)
{
	if (context != nullptr)
		return true;

	context = nfs_init_context();
	if (context == nullptr) {
		error.Set(nfs_domain, "nfs_init_context() failed");
		return false;
	}

	postponed_mount_error.Clear();
	mount_finished = false;
	in_service = false;
	in_event = false;

	if (nfs_mount_async(context, server.c_str(), export_name.c_str(),
			    MountCallback, this) != 0) {
		error.Format(nfs_domain,
			     "nfs_mount_async() failed: %s",
			     nfs_get_error(context));
		nfs_destroy_context(context);
		context = nullptr;
		return false;
	}

	ScheduleSocket();
	return true;
}

void
NfsConnection::BroadcastMountSuccess()
{
	while (!new_leases.empty()) {
		auto i = new_leases.begin();
		active_leases.splice(active_leases.end(), new_leases, i);
		(*i)->OnNfsConnectionReady();
	}
}

void
NfsConnection::BroadcastMountError(Error &&error)
{
	while (!new_leases.empty()) {
		auto l = new_leases.front();
		new_leases.pop_front();
		l->OnNfsConnectionFailed(error);
	}

	OnNfsConnectionError(std::move(error));
}

void
NfsConnection::BroadcastError(Error &&error)
{
	while (!active_leases.empty()) {
		auto l = active_leases.front();
		active_leases.pop_front();
		l->OnNfsConnectionDisconnected(error);
	}

	BroadcastMountError(std::move(error));
}

void
NfsConnection::RunDeferred()
{
	{
		Error error;
		if (!MountInternal(error)) {
			const ScopeLock protect(mutex);
			BroadcastMountError(std::move(error));
			return;
		}
	}

	if (mount_finished) {
		const ScopeLock protect(mutex);
		BroadcastMountSuccess();
	}
}
