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

#ifndef MPD_NFS_CONNECTION_HXX
#define MPD_NFS_CONNECTION_HXX

#include "Cancellable.hxx"
#include "event/SocketEvent.hxx"
#include "event/CoarseTimerEvent.hxx"
#include "event/DeferEvent.hxx"

#include <string>
#include <list>
#include <forward_list>
#include <exception>

struct nfs_context;
struct nfsdir;
struct nfsdirent;
class NfsCallback;
class NfsLease;

/**
 * An asynchronous connection to a NFS server.
 */
class NfsConnection {
	class CancellableCallback : public CancellablePointer<NfsCallback> {
		NfsConnection &connection;

		/**
		 * Is this a nfs_open_async() operation?  If yes, then
		 * we need to call nfs_close_async() on the new file
		 * handle as soon as the callback is invoked
		 * successfully.
		 */
		const bool open;

		/**
		 * The file handle scheduled to be closed as soon as
		 * the operation finishes.
		 */
		struct nfsfh *close_fh;

	public:
		explicit CancellableCallback(NfsCallback &_callback,
					     NfsConnection &_connection,
					     bool _open) noexcept
			:CancellablePointer<NfsCallback>(_callback),
			 connection(_connection),
			 open(_open), close_fh(nullptr) {}

		void Stat(nfs_context *context, const char *path);
		void Lstat(nfs_context *context, const char *path);
		void OpenDirectory(nfs_context *context, const char *path);
		void Open(nfs_context *context, const char *path, int flags);
		void Stat(nfs_context *context, struct nfsfh *fh);
		void Read(nfs_context *context, struct nfsfh *fh,
			  uint64_t offset, size_t size);

		/**
		 * Cancel the operation and schedule a call to
		 * nfs_close_async() with the given file handle.
		 */
		void CancelAndScheduleClose(struct nfsfh *fh) noexcept;

		/**
		 * Called by NfsConnection::DestroyContext() right
		 * before nfs_destroy_context().  This object is given
		 * a chance to prepare for the latter.
		 */
		void PrepareDestroyContext() noexcept;

	private:
		static void Callback(int err, struct nfs_context *nfs,
				     void *data, void *private_data) noexcept;
		void Callback(int err, void *data) noexcept;
	};

	SocketEvent socket_event;
	DeferEvent defer_new_lease;
	CoarseTimerEvent mount_timeout_event;

	std::string server, export_name;

	nfs_context *context;

	typedef std::list<NfsLease *> LeaseList;
	LeaseList new_leases, active_leases;

	typedef CancellableList<NfsCallback, CancellableCallback> CallbackList;
	CallbackList callbacks;

	/**
	 * A list of NFS file handles (struct nfsfh *) which shall be
	 * closed as soon as nfs_service() returns.  If we close the
	 * file handle while in nfs_service(), libnfs may crash, and
	 * deferring this call to after nfs_service() avoids this
	 * problem.
	 */
	std::forward_list<struct nfsfh *> deferred_close;

	std::exception_ptr postponed_mount_error;

#ifndef NDEBUG
	/**
	 * True when nfs_service() is being called.
	 */
	bool in_service;

	/**
	 * True when OnSocketReady() is being called.  During that,
	 * event updates are omitted.
	 */
	bool in_event;

	/**
	 * True when DestroyContext() is being called.
	 */
	bool in_destroy;
#endif

	bool mount_finished;

public:
	[[gnu::nonnull]]
	NfsConnection(EventLoop &_loop,
		      const char *_server, const char *_export_name) noexcept
		:socket_event(_loop, BIND_THIS_METHOD(OnSocketReady)),
		 defer_new_lease(_loop, BIND_THIS_METHOD(RunDeferred)),
		 mount_timeout_event(_loop, BIND_THIS_METHOD(OnMountTimeout)),
		 server(_server), export_name(_export_name),
		 context(nullptr) {}

	/**
	 * Must be run from EventLoop's thread.
	 */
	~NfsConnection() noexcept;

	auto &GetEventLoop() const noexcept {
		return socket_event.GetEventLoop();
	}

	[[gnu::pure]]
	const char *GetServer() const noexcept {
		return server.c_str();
	}

	[[gnu::pure]]
	const char *GetExportName() const noexcept {
		return export_name.c_str();
	}

	/**
	 * Ensure that the connection is established.  The connection
	 * is kept up while at least one #NfsLease is registered.
	 *
	 * This method is thread-safe.  However, #NfsLease's methods
	 * will be invoked from within the #EventLoop's thread.
	 */
	void AddLease(NfsLease &lease) noexcept;
	void RemoveLease(NfsLease &lease) noexcept;

	void Stat(const char *path, NfsCallback &callback);
	void Lstat(const char *path, NfsCallback &callback);

	void OpenDirectory(const char *path, NfsCallback &callback);
	const struct nfsdirent *ReadDirectory(struct nfsdir *dir) noexcept;
	void CloseDirectory(struct nfsdir *dir) noexcept;

	/**
	 * Throws std::runtime_error on error.
	 */
	void Open(const char *path, int flags, NfsCallback &callback);

	void Stat(struct nfsfh *fh, NfsCallback &callback);

	/**
	 * Throws std::runtime_error on error.
	 */
	void Read(struct nfsfh *fh, uint64_t offset, size_t size,
		  NfsCallback &callback);

	void Cancel(NfsCallback &callback) noexcept;

	void Close(struct nfsfh *fh) noexcept;
	void CancelAndClose(struct nfsfh *fh, NfsCallback &callback) noexcept;

protected:
	virtual void OnNfsConnectionError(std::exception_ptr &&e) noexcept = 0;

private:
	void DestroyContext() noexcept;

	/**
	 * Wrapper for nfs_close_async().
	 */
	void InternalClose(struct nfsfh *fh) noexcept;

	/**
	 * Invoke nfs_close_async() after nfs_service() returns.
	 */
	void DeferClose(struct nfsfh *fh) noexcept;

	void MountInternal();
	void BroadcastMountSuccess() noexcept;
	void BroadcastMountError(std::exception_ptr &&e) noexcept;
	void BroadcastError(std::exception_ptr &&e) noexcept;

	static void MountCallback(int status, nfs_context *nfs, void *data,
				  void *private_data) noexcept;
	void MountCallback(int status, nfs_context *nfs, void *data) noexcept;

	void ScheduleSocket() noexcept;

	/**
	 * Wrapper for nfs_service().
	 */
	int Service(unsigned flags) noexcept;

	void OnSocketReady(unsigned flags) noexcept;

	/* callback for #mount_timeout_event */
	void OnMountTimeout() noexcept;

	/* DeferEvent callback */
	void RunDeferred() noexcept;
};

#endif
