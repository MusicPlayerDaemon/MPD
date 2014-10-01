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
#include "thread/Mutex.hxx"
#include "event/SocketMonitor.hxx"
#include "event/DeferredMonitor.hxx"
#include "util/Error.hxx"

#include <string>
#include <list>

struct nfs_context;
class NfsCallback;

/**
 * An asynchronous connection to a NFS server.
 */
class NfsConnection : SocketMonitor, DeferredMonitor {
	class CancellableCallback : public CancellablePointer<NfsCallback> {
		NfsConnection &connection;

	public:
		explicit CancellableCallback(NfsCallback &_callback,
					     NfsConnection &_connection)
			:CancellablePointer<NfsCallback>(_callback),
			 connection(_connection) {}

		bool Open(nfs_context *context, const char *path, int flags,
			  Error &error);
		bool Stat(nfs_context *context, struct nfsfh *fh,
			  Error &error);
		bool Read(nfs_context *context, struct nfsfh *fh,
			  uint64_t offset, size_t size,
			  Error &error);

	private:
		static void Callback(int err, struct nfs_context *nfs,
				     void *data, void *private_data);
		void Callback(int err, void *data);
	};

	std::string server, export_name;

	nfs_context *context;

	Mutex mutex;

	typedef std::list<NfsLease *> LeaseList;
	LeaseList new_leases, active_leases;

	typedef CancellableList<NfsCallback, CancellableCallback> CallbackList;
	CallbackList callbacks;

	Error postponed_mount_error;

	/**
	 * True when nfs_service() is being called.  During that,
	 * nfs_client_free() is postponed, or libnfs will crash.  See
	 * #postponed_destroy.
	 */
	bool in_service;

	/**
	 * True when OnSocketReady() is being called.  During that,
	 * event updates are omitted.
	 */
	bool in_event;

	/**
	 * True when nfs_client_free() has been called while #in_service
	 * was true.
	 */
	bool postponed_destroy;

	bool mount_finished;

public:
	gcc_nonnull_all
	NfsConnection(EventLoop &_loop,
		      const char *_server, const char *_export_name)
		:SocketMonitor(_loop), DeferredMonitor(_loop),
		 server(_server), export_name(_export_name),
		 context(nullptr) {}

#if defined(__GNUC__) && !defined(__clang__) && !GCC_CHECK_VERSION(4,8)
	/* needed for NfsManager::GetConnection() due to lack of
	   std::map::emplace() */
	NfsConnection(NfsConnection &&other)
		:SocketMonitor(((SocketMonitor &)other).GetEventLoop()),
		 DeferredMonitor(((DeferredMonitor &)other).GetEventLoop()),
		 server(std::move(other.server)),
		 export_name(std::move(other.export_name)),
		 context(nullptr) {
		assert(other.context == nullptr);
		assert(other.new_leases.empty());
		assert(other.active_leases.empty());
		assert(other.callbacks.IsEmpty());
	}
#endif

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

	/**
	 * Ensure that the connection is established.  The connection
	 * is kept up while at least one #NfsLease is registered.
	 *
	 * This method is thread-safe.  However, #NfsLease's methods
	 * will be invoked from within the #EventLoop's thread.
	 */
	void AddLease(NfsLease &lease);
	void RemoveLease(NfsLease &lease);

	bool Open(const char *path, int flags, NfsCallback &callback,
		  Error &error);
	bool Stat(struct nfsfh *fh, NfsCallback &callback, Error &error);
	bool Read(struct nfsfh *fh, uint64_t offset, size_t size,
		  NfsCallback &callback, Error &error);
	void Cancel(NfsCallback &callback);

	void Close(struct nfsfh *fh);

protected:
	virtual void OnNfsConnectionError(Error &&error) = 0;

private:
	void DestroyContext();
	bool MountInternal(Error &error);
	void BroadcastMountSuccess();
	void BroadcastMountError(Error &&error);
	void BroadcastError(Error &&error);

	static void MountCallback(int status, nfs_context *nfs, void *data,
				  void *private_data);
	void MountCallback(int status, nfs_context *nfs, void *data);

	void ScheduleSocket();

	/* virtual methods from SocketMonitor */
	virtual bool OnSocketReady(unsigned flags) override;

	/* virtual methods from DeferredMonitor */
	virtual void RunDeferred() override;
};

#endif
