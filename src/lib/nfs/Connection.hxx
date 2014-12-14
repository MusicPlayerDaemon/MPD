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

#ifndef MPD_NFS_CONNECTION_HXX
#define MPD_NFS_CONNECTION_HXX

#include "Lease.hxx"
#include "Cancellable.hxx"
#include "event/SocketMonitor.hxx"
#include "event/TimeoutMonitor.hxx"
#include "event/DeferredMonitor.hxx"
#include "util/Error.hxx"

#include <boost/intrusive/list.hpp>

#include <string>
#include <list>
#include <forward_list>

struct nfs_context;
struct nfsdir;
struct nfsdirent;
class NfsCallback;

/**
 * An asynchronous connection to a NFS server.
 */
class NfsConnection : SocketMonitor, TimeoutMonitor, DeferredMonitor {
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
					     bool _open)
			:CancellablePointer<NfsCallback>(_callback),
			 connection(_connection),
			 open(_open), close_fh(nullptr) {}

		bool Stat(nfs_context *context, const char *path,
			  Error &error);
		bool OpenDirectory(nfs_context *context, const char *path,
				   Error &error);
		bool Open(nfs_context *context, const char *path, int flags,
			  Error &error);
		bool Stat(nfs_context *context, struct nfsfh *fh,
			  Error &error);
		bool Read(nfs_context *context, struct nfsfh *fh,
			  uint64_t offset, size_t size,
			  Error &error);

		/**
		 * Cancel the operation and schedule a call to
		 * nfs_close_async() with the given file handle.
		 */
		void CancelAndScheduleClose(struct nfsfh *fh);

		/**
		 * Called by NfsConnection::DestroyContext() right
		 * before nfs_destroy_context().  This object is given
		 * a chance to prepare for the latter.
		 */
		void PrepareDestroyContext();

	private:
		static void Callback(int err, struct nfs_context *nfs,
				     void *data, void *private_data);
		void Callback(int err, void *data);
	};

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

	Error postponed_mount_error;

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
	gcc_nonnull_all
	NfsConnection(EventLoop &_loop,
		      const char *_server, const char *_export_name)
		:SocketMonitor(_loop), TimeoutMonitor(_loop),
		 DeferredMonitor(_loop),
		 server(_server), export_name(_export_name),
		 context(nullptr) {}

	/**
	 * Must be run from EventLoop's thread.
	 */
	~NfsConnection();

	gcc_pure
	const char *GetServer() const {
		return server.c_str();
	}

	gcc_pure
	const char *GetExportName() const {
		return export_name.c_str();
	}

	EventLoop &GetEventLoop() {
		return SocketMonitor::GetEventLoop();
	}

	/**
	 * Ensure that the connection is established.  The connection
	 * is kept up while at least one #NfsLease is registered.
	 *
	 * This method is thread-safe.  However, #NfsLease's methods
	 * will be invoked from within the #EventLoop's thread.
	 */
	void AddLease(NfsLease &lease);
	void RemoveLease(NfsLease &lease);

	bool Stat(const char *path, NfsCallback &callback, Error &error);

	bool OpenDirectory(const char *path, NfsCallback &callback,
			   Error &error);
	const struct nfsdirent *ReadDirectory(struct nfsdir *dir);
	void CloseDirectory(struct nfsdir *dir);

	bool Open(const char *path, int flags, NfsCallback &callback,
		  Error &error);
	bool Stat(struct nfsfh *fh, NfsCallback &callback, Error &error);
	bool Read(struct nfsfh *fh, uint64_t offset, size_t size,
		  NfsCallback &callback, Error &error);
	void Cancel(NfsCallback &callback);

	void Close(struct nfsfh *fh);
	void CancelAndClose(struct nfsfh *fh, NfsCallback &callback);

protected:
	virtual void OnNfsConnectionError(Error &&error) = 0;

private:
	void DestroyContext();

	/**
	 * Wrapper for nfs_close_async().
	 */
	void InternalClose(struct nfsfh *fh);

	/**
	 * Invoke nfs_close_async() after nfs_service() returns.
	 */
	void DeferClose(struct nfsfh *fh);

	bool MountInternal(Error &error);
	void BroadcastMountSuccess();
	void BroadcastMountError(Error &&error);
	void BroadcastError(Error &&error);

	static void MountCallback(int status, nfs_context *nfs, void *data,
				  void *private_data);
	void MountCallback(int status, nfs_context *nfs, void *data);

	void ScheduleSocket();

	/**
	 * Wrapper for nfs_service().
	 */
	int Service(unsigned flags);

	/* virtual methods from SocketMonitor */
	virtual bool OnSocketReady(unsigned flags) override;

	/* virtual methods from TimeoutMonitor */
	void OnTimeout() final;

	/* virtual methods from DeferredMonitor */
	virtual void RunDeferred() override;
};

#endif
