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

#include "UdisksNeighborPlugin.hxx"
#include "lib/dbus/Connection.hxx"
#include "lib/dbus/Error.hxx"
#include "lib/dbus/Glue.hxx"
#include "lib/dbus/FilterHelper.hxx"
#include "lib/dbus/Message.hxx"
#include "lib/dbus/AsyncRequest.hxx"
#include "lib/dbus/ReadIter.hxx"
#include "lib/dbus/ObjectManager.hxx"
#include "lib/dbus/UDisks2.hxx"
#include "neighbor/NeighborPlugin.hxx"
#include "neighbor/Explorer.hxx"
#include "neighbor/Listener.hxx"
#include "neighbor/Info.hxx"
#include "event/Call.hxx"
#include "thread/Mutex.hxx"
#include "thread/SafeSingleton.hxx"
#include "util/Manual.hxx"
#include "Log.hxx"

#include <string>
#include <map>

static NeighborInfo
ToNeighborInfo(const UDisks2::Object &o) noexcept
{
	return {o.GetUri(), o.path};
}

static constexpr char udisks_neighbor_match[] =
	"type='signal',sender='" UDISKS2_INTERFACE "',"
	"interface='" DBUS_OM_INTERFACE "',"
	"path='" UDISKS2_PATH "'";

class UdisksNeighborExplorer final
	: public NeighborExplorer {

	EventLoop &event_loop;

	Manual<SafeSingleton<ODBus::Glue>> dbus_glue;

	ODBus::FilterHelper filter_helper;

	ODBus::AsyncRequest list_request;

	/**
	 * Protects #by_uri, #by_path.
	 */
	mutable Mutex mutex;

	using ByUri = std::map<std::string, NeighborInfo>;
	ByUri by_uri;
	std::map<std::string, ByUri::iterator> by_path;

public:
	UdisksNeighborExplorer(EventLoop &_event_loop,
			       NeighborListener &_listener) noexcept
		:NeighborExplorer(_listener), event_loop(_event_loop) {}

	auto &GetEventLoop() const noexcept {
		return event_loop;
	}

	auto &&GetConnection() noexcept {
		return dbus_glue.Get()->GetConnection();
	}

	/* virtual methods from class NeighborExplorer */
	void Open() override;
	void Close() noexcept override;
	List GetList() const noexcept override;

private:
	void DoOpen();
	void DoClose() noexcept;

	void Insert(UDisks2::Object &&o) noexcept;
	void Remove(const std::string &path) noexcept;

	void OnListNotify(ODBus::Message reply) noexcept;

	DBusHandlerResult HandleMessage(DBusConnection *dbus_connection,
					DBusMessage *message) noexcept;
};

inline void
UdisksNeighborExplorer::DoOpen()
{
	using namespace ODBus;

	dbus_glue.Construct(event_loop);

	auto &connection = GetConnection();

	/* this ugly try/catch cascade is only here because this
	   method has no RAII for this method - TODO: improve this */
	try {
		Error error;
		dbus_bus_add_match(connection, udisks_neighbor_match, error);
		error.CheckThrow("DBus AddMatch error");

		try {
			filter_helper.Add(connection,
					  BIND_THIS_METHOD(HandleMessage));

			try {
				auto msg = Message::NewMethodCall(UDISKS2_INTERFACE,
								  UDISKS2_PATH,
								  DBUS_OM_INTERFACE,
								  "GetManagedObjects");
				list_request.Send(connection, *msg.Get(), [this](auto o) {
					return OnListNotify(std::move(o));
				});
			} catch (...) {
				filter_helper.Remove();
				throw;
			}
		} catch (...) {
			dbus_bus_remove_match(connection,
					      udisks_neighbor_match, nullptr);
			throw;
		}
	} catch (...) {
		dbus_glue.Destruct();
		throw;
	}
}

void
UdisksNeighborExplorer::Open()
{
	BlockingCall(GetEventLoop(), [this](){ DoOpen(); });
}

inline void
UdisksNeighborExplorer::DoClose() noexcept
{
	if (list_request) {
		list_request.Cancel();
	}

	auto &connection = GetConnection();

	filter_helper.Remove();
	dbus_bus_remove_match(connection, udisks_neighbor_match, nullptr);

	dbus_glue.Destruct();
}

void
UdisksNeighborExplorer::Close() noexcept
{
	BlockingCall(GetEventLoop(), [this](){ DoClose(); });
}

NeighborExplorer::List
UdisksNeighborExplorer::GetList() const noexcept
{
	const std::scoped_lock<Mutex> lock(mutex);

	NeighborExplorer::List result;

	for (const auto &[t, r] : by_uri)
		result.emplace_front(r);
	return result;
}

void
UdisksNeighborExplorer::Insert(UDisks2::Object &&o) noexcept
{
	assert(o.IsValid());

	const NeighborInfo info = ToNeighborInfo(o);

	{
		const std::scoped_lock<Mutex> protect(mutex);
		auto i = by_uri.emplace(o.GetUri(), info);
		if (!i.second)
			i.first->second = info;

		by_path.emplace(o.path, i.first);
		// TODO: do we need to remove a conflicting path?
	}

	listener.FoundNeighbor(info);
}

void
UdisksNeighborExplorer::Remove(const std::string &path) noexcept
{
	std::unique_lock<Mutex> lock(mutex);

	auto i = by_path.find(path);
	if (i == by_path.end())
		return;

	const auto info = std::move(i->second->second);

	by_uri.erase(i->second);
	by_path.erase(i);

	lock.unlock();
	listener.LostNeighbor(info);
}

inline void
UdisksNeighborExplorer::OnListNotify(ODBus::Message reply) noexcept
{
	try{
		UDisks2::ParseObjects(reply,
				      [this](auto p) { return Insert(std::move(p)); });
	} catch (...) {
		LogError(std::current_exception(),
			 "Failed to parse GetManagedObjects reply");
		return;
	}
}

inline DBusHandlerResult
UdisksNeighborExplorer::HandleMessage(DBusConnection *, DBusMessage *message) noexcept
{
	using namespace ODBus;

	if (dbus_message_is_signal(message, DBUS_OM_INTERFACE,
				   "InterfacesAdded") &&
	    dbus_message_has_signature(message, InterfacesAddedType::as_string)) {
		RecurseInterfaceDictEntry(ReadMessageIter(*message), [this](const char *path, auto &&i){
				UDisks2::Object o(path);
				UDisks2::ParseObject(o, std::forward<decltype(i)>(i));
				if (o.IsValid())
					this->Insert(std::move(o));
			});

		return DBUS_HANDLER_RESULT_HANDLED;
	} else if (dbus_message_is_signal(message, DBUS_OM_INTERFACE,
					  "InterfacesRemoved") &&
		   dbus_message_has_signature(message, InterfacesRemovedType::as_string)) {
		Remove(ReadMessageIter(*message).GetString());
		return DBUS_HANDLER_RESULT_HANDLED;
	} else
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static std::unique_ptr<NeighborExplorer>
udisks_neighbor_create(EventLoop &event_loop,
		     NeighborListener &listener,
		     [[maybe_unused]] const ConfigBlock &block)
{
	return std::make_unique<UdisksNeighborExplorer>(event_loop, listener);
}

const NeighborPlugin udisks_neighbor_plugin = {
	"udisks",
	udisks_neighbor_create,
};
