// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include "Cancellable.hxx"
#include "event/SocketEvent.hxx"
#include "event/CoarseTimerEvent.hxx"
#include "event/DeferEvent.hxx"
#include "util/DisposablePointer.hxx"
#include "util/IntrusiveList.hxx"

#include <cstdint>
#include <string>
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

		/**
		 * An arbitrary value that will be disposed of after
		 * cancellation completes.
		 */
		DisposablePointer dispose_value;

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
			  uint64_t offset,
#ifdef LIBNFS_API_2
			  std::span<std::byte> dest
#else
			  std::size_t size
#endif
			);

		/**
		 * Cancel the operation and schedule a call to
		 * nfs_close_async() with the given file handle.
		 */
		void CancelAndScheduleClose(struct nfsfh *fh,
					    DisposablePointer &&_dispose_value) noexcept;

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

	const std::string server, export_name;

	nfs_context *const context;

	using LeaseList = IntrusiveList<NfsLease>;
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

	enum class MountState : uint_least8_t {
		INITIAL,
		WAITING,
		FINISHED,
	} mount_state = MountState::INITIAL;

#ifndef NDEBUG
	/**
	 * True when nfs_service() is being called.
	 */
	bool in_service = false;

	/**
	 * True when OnSocketReady() is being called.  During that,
	 * event updates are omitted.
	 */
	bool in_event = false;
#endif

public:
	/**
	 * Throws on error.
	 */
	[[gnu::nonnull]]
	NfsConnection(EventLoop &_loop,
		      nfs_context *_context,
		      std::string_view _server,
		      std::string_view _export_name);

	/**
	 * Must be run from EventLoop's thread.
	 */
	~NfsConnection() noexcept;

	auto &GetEventLoop() const noexcept {
		return socket_event.GetEventLoop();
	}

	[[gnu::pure]]
	std::string_view GetServer() const noexcept {
		return server;
	}

	[[gnu::pure]]
	std::string_view GetExportName() const noexcept {
		return export_name;
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

	/**
	 * Throws on error.
	 */
	void Stat(const char *path, NfsCallback &callback);

	/**
	 * Throws on error.
	 */
	void Lstat(const char *path, NfsCallback &callback);

	/**
	 * Throws on error.
	 */
	void OpenDirectory(const char *path, NfsCallback &callback);

	/**
	 * Read the next entry from the specified directory.
	 *
	 * Unlike the other I/O methods, this method blocks (because
	 * libnfs has no non-blocking variant of nfs_readdir()) and
	 * does not throw an exception on error.
	 */
	const struct nfsdirent *ReadDirectory(struct nfsdir *dir) noexcept;

	/**
	 * Close a directory handle returned by OpenDirectory().  This
	 * method never blocks and never fails.
	 */
	void CloseDirectory(struct nfsdir *dir) noexcept;

	/**
	 * Throws on error.
	 */
	void Open(const char *path, int flags, NfsCallback &callback);

	/**
	 * Throws on error.
	 */
	void Stat(struct nfsfh *fh, NfsCallback &callback);

	/**
	 * Throws on error.
	 */
	void Read(struct nfsfh *fh, uint64_t offset,
#ifdef LIBNFS_API_2
		  std::span<std::byte> dest,
#else
		  std::size_t size,
#endif
		  NfsCallback &callback);

	/**
	 * Cancel the asynchronous operation associated with the
	 * specified #NfsCallback.
	 *
	 * After this method returns, the caller may delete the
	 * #NfsCallback.
	 *
	 * Not thread-safe.
	 *
	 * @param fh if not nullptr, then close this NFS file handle
	 * after cancellation completes
	 * @param dispose_value an arbitrary value that will be
	 * disposed of after cancellation completes
	 */
	void Cancel(NfsCallback &callback,
		    struct nfsfh *fh, DisposablePointer dispose_value) noexcept;

	/**
	 * Close the specified file handle asynchronously.
	 *
	 * Not thread-safe.
	 */
	void Close(struct nfsfh *fh) noexcept;

protected:
	virtual void OnNfsConnectionError(std::exception_ptr e) noexcept = 0;

private:
	void PrepareDestroyContext() noexcept;

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
	void BroadcastMountError(std::exception_ptr e) noexcept;
	void BroadcastError(std::exception_ptr e) noexcept;

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
