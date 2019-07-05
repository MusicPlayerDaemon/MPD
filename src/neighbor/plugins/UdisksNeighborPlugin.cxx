/*
 * Copyright 2003-2019 The Music Player Daemon Project
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

class UdisksNeighborExplorer final
	: public NeighborExplorer {

	EventLoop &event_loop;

	Manual<SafeSingleton<ODBus::Glue>> dbus_glue;

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
	static DBusHandlerResult HandleMessage(DBusConnection *dbus_connection,
					       DBusMessage *message,
					       void *user_data) noexcept;
};

inline void
UdisksNeighborExplorer::DoOpen()
{
	using namespace ODBus;

	dbus_glue.Construct(event_loop);

	auto &connection = GetConnection();

	try {
		Error error;
		dbus_bus_add_match(connection,
				   "type='signal',sender='" UDISKS2_INTERFACE "',"
				   "interface='" DBUS_OM_INTERFACE "',"
				   "path='" UDISKS2_PATH "'",
				   error);
		error.CheckThrow("DBus AddMatch error");

		dbus_connection_add_filter(connection,
					   HandleMessage, this,
					   nullptr);

		auto msg = Message::NewMethodCall(UDISKS2_INTERFACE,
						  UDISKS2_PATH,
						  DBUS_OM_INTERFACE,
						  "GetManagedObjects");
		list_request.Send(connection, *msg.Get(),
				  std::bind(&UdisksNeighborExplorer::OnListNotify,
					    this, std::placeholders::_1));
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

	// TODO: remove_match
	// TODO: remove_filter

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
	const std::lock_guard<Mutex> lock(mutex);

	NeighborExplorer::List result;

	for (const auto &i : by_uri)
		result.emplace_front(i.second);
	return result;
}

void
UdisksNeighborExplorer::Insert(UDisks2::Object &&o) noexcept
{
	assert(o.IsValid());

	const NeighborInfo info = ToNeighborInfo(o);

	{
		const std::lock_guard<Mutex> protect(mutex);
		auto i = by_uri.emplace(std::make_pair(o.GetUri(), info));
		if (!i.second)
			i.first->second = info;

		by_path.emplace(std::make_pair(o.path, i.first));
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
		ParseObjects(reply,
			     std::bind(&UdisksNeighborExplorer::Insert,
				       this, std::placeholders::_1));
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
	    dbus_message_has_signature(message, InterfacesAddedType::value)) {
		RecurseInterfaceDictEntry(ReadMessageIter(*message), [this](const char *path, auto &&i){
				UDisks2::Object o(path);
				UDisks2::ParseObject(o, std::move(i));
				if (o.IsValid())
					this->Insert(std::move(o));
			});

		return DBUS_HANDLER_RESULT_HANDLED;
	} else if (dbus_message_is_signal(message, DBUS_OM_INTERFACE,
					  "InterfacesRemoved") &&
		   dbus_message_has_signature(message, InterfacesRemovedType::value)) {
		Remove(ReadMessageIter(*message).GetString());
		return DBUS_HANDLER_RESULT_HANDLED;
	} else
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

DBusHandlerResult
UdisksNeighborExplorer::HandleMessage(DBusConnection *connection,
				      DBusMessage *message,
				      void *user_data) noexcept
{
	auto &agent = *(UdisksNeighborExplorer *)user_data;

	return agent.HandleMessage(connection, message);
}

static std::unique_ptr<NeighborExplorer>
udisks_neighbor_create(EventLoop &event_loop,
		     NeighborListener &listener,
		     gcc_unused const ConfigBlock &block)
{
	return std::make_unique<UdisksNeighborExplorer>(event_loop, listener);
}

const NeighborPlugin udisks_neighbor_plugin = {
	"udisks",
	udisks_neighbor_create,
};
