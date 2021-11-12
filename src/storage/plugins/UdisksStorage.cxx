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

#include "UdisksStorage.hxx"
#include "LocalStorage.hxx"
#include "storage/StoragePlugin.hxx"
#include "storage/StorageInterface.hxx"
#include "storage/FileInfo.hxx"
#include "lib/fmt/ExceptionFormatter.hxx"
#include "lib/dbus/Glue.hxx"
#include "lib/dbus/AsyncRequest.hxx"
#include "lib/dbus/Message.hxx"
#include "lib/dbus/AppendIter.hxx"
#include "lib/dbus/ReadIter.hxx"
#include "lib/dbus/ObjectManager.hxx"
#include "lib/dbus/UDisks2.hxx"
#include "thread/Mutex.hxx"
#include "thread/Cond.hxx"
#include "thread/SafeSingleton.hxx"
#include "event/Call.hxx"
#include "event/InjectEvent.hxx"
#include "fs/AllocatedPath.hxx"
#include "util/Domain.hxx"
#include "util/StringCompare.hxx"
#include "util/RuntimeError.hxx"
#include "Log.hxx"

#include <stdexcept>

static constexpr Domain udisks_domain("udisks");

class UdisksStorage final : public Storage {
	const std::string base_uri;
	const std::string id;

	const AllocatedPath inside_path;

	std::string dbus_path;

	SafeSingleton<ODBus::Glue> dbus_glue;
	ODBus::AsyncRequest list_request;
	ODBus::AsyncRequest mount_request;

	mutable Mutex mutex;
	Cond cond;

	bool want_mount = false;

	std::unique_ptr<Storage> mounted_storage;

	std::exception_ptr mount_error;

	InjectEvent defer_mount, defer_unmount;

public:
	template<typename B, typename I, typename IP>
	UdisksStorage(EventLoop &_event_loop, B &&_base_uri, I &&_id,
		      IP &&_inside_path)
		:base_uri(std::forward<B>(_base_uri)),
		 id(std::forward<I>(_id)),
		 inside_path(std::forward<IP>(_inside_path)),
		 dbus_glue(_event_loop),
		 defer_mount(_event_loop, BIND_THIS_METHOD(DeferredMount)),
		 defer_unmount(_event_loop, BIND_THIS_METHOD(DeferredUnmount)) {}

	~UdisksStorage() noexcept override {
		if (list_request || mount_request)
			BlockingCall(GetEventLoop(), [this](){
					if (list_request)
						list_request.Cancel();
					if (mount_request)
						mount_request.Cancel();
				});

		try {
			UnmountWait();
		} catch (...) {
			FmtError(udisks_domain,
				 "Failed to unmount '{}': {}",
				 base_uri, std::current_exception());
		}
	}

	UdisksStorage(const UdisksStorage &) = delete;
	UdisksStorage &operator=(const UdisksStorage &) = delete;

	EventLoop &GetEventLoop() const noexcept {
		return defer_mount.GetEventLoop();
	}

	/* virtual methods from class Storage */
	StorageFileInfo GetInfo(std::string_view uri_utf8, bool follow) override {
		MountWait();
		return mounted_storage->GetInfo(uri_utf8, follow);
	}

	std::unique_ptr<StorageDirectoryReader> OpenDirectory(std::string_view uri_utf8) override {
		MountWait();
		return mounted_storage->OpenDirectory(uri_utf8);
	}

	std::string MapUTF8(std::string_view uri_utf8) const noexcept override;

	AllocatedPath MapFS(std::string_view uri_utf8) const noexcept override {
		try {
			const_cast<UdisksStorage *>(this)->MountWait();
		} catch (...) {
			return nullptr;
		}

		return mounted_storage->MapFS(uri_utf8);
	}

	std::string_view MapToRelativeUTF8(std::string_view uri_utf8) const noexcept override;

private:
	void SetMountPoint(Path mount_point);
	void LockSetMountPoint(Path mount_point);

	void OnListReply(ODBus::Message reply) noexcept;

	void MountWait();
	void DeferredMount() noexcept;
	void OnMountNotify(ODBus::Message reply) noexcept;

	void UnmountWait();
	void DeferredUnmount() noexcept;
	void OnUnmountNotify(ODBus::Message reply) noexcept;
};

inline void
UdisksStorage::SetMountPoint(Path mount_point)
{
	mounted_storage = inside_path.IsNull()
		? CreateLocalStorage(mount_point)
		: CreateLocalStorage(mount_point / inside_path);

	mount_error = {};
	want_mount = false;
	cond.notify_all();
}

void
UdisksStorage::LockSetMountPoint(Path mount_point)
{
	const std::scoped_lock<Mutex> lock(mutex);
	SetMountPoint(mount_point);
}

void
UdisksStorage::OnListReply(ODBus::Message reply) noexcept
{
	using namespace UDisks2;

	try {
		std::string mount_point;

		ParseObjects(reply, [this, &mount_point](Object &&o) {
			if (!o.IsId(id))
				return;

			dbus_path = std::move(o.path);
			mount_point = std::move(o.mount_point);
		});

		if (dbus_path.empty())
			throw FormatRuntimeError("No such UDisks2 object: %s",
						 id.c_str());

		if (!mount_point.empty()) {
			/* already mounted: don't attempt to mount
			   again, because this would result in
			   org.freedesktop.UDisks2.Error.AlreadyMounted */
			LockSetMountPoint(Path::FromFS(mount_point.c_str()));
			return;
		}
	} catch (...) {
		const std::scoped_lock<Mutex> lock(mutex);
		mount_error = std::current_exception();
		want_mount = false;
		cond.notify_all();
		return;
	}

	DeferredMount();
}

void
UdisksStorage::MountWait()
{
	std::unique_lock<Mutex> lock(mutex);

	if (mounted_storage)
		/* already mounted */
		return;

	if (!want_mount) {
		want_mount = true;
		defer_mount.Schedule();
	}

	cond.wait(lock, [this]{ return !want_mount; });

	if (mount_error)
		std::rethrow_exception(mount_error);
}

void
UdisksStorage::DeferredMount() noexcept
try {
	using namespace ODBus;

	auto &connection = dbus_glue->GetConnection();

	if (dbus_path.empty()) {
		auto msg = Message::NewMethodCall(UDISKS2_INTERFACE,
						  UDISKS2_PATH,
						  DBUS_OM_INTERFACE,
						  "GetManagedObjects");
		list_request.Send(connection, *msg.Get(),
				  [this](auto o) { return OnListReply(std::move(o)); });
		return;
	}

	auto msg = Message::NewMethodCall(UDISKS2_INTERFACE,
					  dbus_path.c_str(),
					  UDISKS2_FILESYSTEM_INTERFACE,
					  "Mount");
	AppendMessageIter(*msg.Get()).AppendEmptyArray<DictEntryTypeTraits<StringTypeTraits, VariantTypeTraits>>();

	mount_request.Send(connection, *msg.Get(),
			   [this](auto o) { return OnMountNotify(std::move(o)); });
} catch (...) {
	const std::scoped_lock<Mutex> lock(mutex);
	mount_error = std::current_exception();
	want_mount = false;
	cond.notify_all();
}

void
UdisksStorage::OnMountNotify(ODBus::Message reply) noexcept
try {
	using namespace ODBus;
	reply.CheckThrowError();

	ReadMessageIter i(*reply.Get());
	if (i.GetArgType() != DBUS_TYPE_STRING)
		throw std::runtime_error("Malformed 'Mount' response");

	const char *mount_path = i.GetString();
	LockSetMountPoint(Path::FromFS(mount_path));
} catch (...) {
	const std::scoped_lock<Mutex> lock(mutex);
	mount_error = std::current_exception();
	want_mount = false;
	cond.notify_all();
}

void
UdisksStorage::UnmountWait()
{
	std::unique_lock<Mutex> lock(mutex);

	if (!mounted_storage)
		/* not mounted */
		return;

	defer_unmount.Schedule();

	cond.wait(lock, [this]{ return !mounted_storage; });

	if (mount_error)
		std::rethrow_exception(mount_error);
}

void
UdisksStorage::DeferredUnmount() noexcept
try {
	using namespace ODBus;

	auto &connection = dbus_glue->GetConnection();
	auto msg = Message::NewMethodCall(UDISKS2_INTERFACE,
					  dbus_path.c_str(),
					  UDISKS2_FILESYSTEM_INTERFACE,
					  "Unmount");
	AppendMessageIter(*msg.Get()).AppendEmptyArray<DictEntryTypeTraits<StringTypeTraits, VariantTypeTraits>>();

	mount_request.Send(connection, *msg.Get(),
			   [this](auto u) { return OnUnmountNotify(std::move(u)); });
} catch (...) {
	const std::scoped_lock<Mutex> lock(mutex);
	mount_error = std::current_exception();
	mounted_storage.reset();
	cond.notify_all();
}

void
UdisksStorage::OnUnmountNotify(ODBus::Message reply) noexcept
try {
	using namespace ODBus;
	reply.CheckThrowError();

	const std::scoped_lock<Mutex> lock(mutex);
	mount_error = {};
	mounted_storage.reset();
	cond.notify_all();
} catch (...) {
	const std::scoped_lock<Mutex> lock(mutex);
	mount_error = std::current_exception();
	mounted_storage.reset();
	cond.notify_all();
}

std::string
UdisksStorage::MapUTF8(std::string_view uri_utf8) const noexcept
{
	if (uri_utf8.empty())
		/* kludge for a special case: return the "udisks://"
		   URI if the parameter is an empty string to fix the
		   mount URIs in the state file */
		return base_uri;

	try {
		const_cast<UdisksStorage *>(this)->MountWait();

		return mounted_storage->MapUTF8(uri_utf8);
	} catch (...) {
		/* fallback - not usable but the best we can do */
		return PathTraitsUTF8::Build(base_uri, uri_utf8);
	}
}

std::string_view
UdisksStorage::MapToRelativeUTF8(std::string_view uri_utf8) const noexcept
{
	return PathTraitsUTF8::Relative(base_uri, uri_utf8);
}

static std::unique_ptr<Storage>
CreateUdisksStorageURI(EventLoop &event_loop, const char *base_uri)
{
	const char *id_begin = StringAfterPrefix(base_uri, "udisks://");
	if (id_begin == nullptr)
		return nullptr;

	std::string id;

	const char *relative_path = std::strchr(id_begin, '/');
	if (relative_path == nullptr) {
		id = id_begin;
		relative_path = "";
	} else {
		id = {id_begin, relative_path};
		++relative_path;
		while (*relative_path == '/')
			++relative_path;
	}

	auto inside_path = *relative_path != 0
		? AllocatedPath::FromUTF8Throw(relative_path)
		: nullptr;

	return std::make_unique<UdisksStorage>(event_loop, base_uri,
					       std::move(id),
					       std::move(inside_path));
}

static constexpr const char *udisks_prefixes[] = { "udisks://", nullptr };

const StoragePlugin udisks_storage_plugin = {
	"udisks",
	udisks_prefixes,
	CreateUdisksStorageURI,
};
