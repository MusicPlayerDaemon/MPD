/*
 * Copyright 2003-2018 The Music Player Daemon Project
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
#include "UdisksNeighborPlugin.hxx"
#include "lib/dbus/Connection.hxx"
#include "lib/dbus/Error.hxx"
#include "lib/dbus/Glue.hxx"
#include "lib/dbus/Message.hxx"
#include "lib/dbus/PendingCall.hxx"
#include "lib/dbus/ReadIter.hxx"
#include "lib/dbus/ObjectManager.hxx"
#include "lib/dbus/UDisks2.hxx"
#include "neighbor/NeighborPlugin.hxx"
#include "neighbor/Explorer.hxx"
#include "neighbor/Listener.hxx"
#include "neighbor/Info.hxx"
#include "thread/Mutex.hxx"
#include "util/Domain.hxx"
#include "util/StringAPI.hxx"
#include "util/Manual.hxx"
#include "Log.hxx"

#include <stdexcept>
#include <string>
#include <list>
#include <map>

static constexpr Domain udisks_domain("udisks");

struct UdisksObject {
	std::string path;

	std::string drive_id, block_id;

	bool is_filesystem = false;

	bool IsValid() const noexcept {
		return !drive_id.empty() || !block_id.empty();
	}

	std::string GetUri() const noexcept {
		if (!drive_id.empty())
			return "udisks://" + drive_id;
		else if (!block_id.empty())
			return "udisks://" + block_id;
		else
			return {};
	}

	NeighborInfo ToNeighborInfo() const noexcept {
		return {GetUri(), path};
	}
};

class UdisksNeighborExplorer final
	: public NeighborExplorer {

	EventLoop &event_loop;

	Manual<ODBus::Glue> dbus_glue;

	ODBus::PendingCall pending_list_call;

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

	auto &GetEventLoop() noexcept {
		return event_loop;
	}

	auto &&GetConnection() noexcept {
		return dbus_glue->GetConnection();
	}

	/* virtual methods from class NeighborExplorer */
	void Open() override;
	void Close() noexcept override;
	List GetList() const noexcept override;

private:
	void Insert(UdisksObject &&o) noexcept;
	void Remove(const std::string &path) noexcept;

	void OnListNotify(DBusPendingCall *pending) noexcept;

	static void OnListNotify(DBusPendingCall *pending,
				 void *user_data) noexcept {
		auto &e = *(UdisksNeighborExplorer *)user_data;
		e.OnListNotify(pending);
	}

	DBusHandlerResult HandleMessage(DBusConnection *dbus_connection,
					DBusMessage *message) noexcept;
	static DBusHandlerResult HandleMessage(DBusConnection *dbus_connection,
					       DBusMessage *message,
					       void *user_data) noexcept;
};

void
UdisksNeighborExplorer::Open()
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
		pending_list_call = PendingCall::SendWithReply(connection, msg.Get());
		pending_list_call.SetNotify(OnListNotify, this);
	} catch (...) {
		dbus_glue.Destruct();
		throw;
	}
}

void
UdisksNeighborExplorer::Close() noexcept
{
	using namespace ODBus;

	if (pending_list_call) {
		pending_list_call.Cancel();
	}

	// TODO: remove_match
	// TODO: remove_filter

	dbus_glue.Destruct();
}

template<typename I>
gcc_pure
static const char *
CheckString(I &&i) noexcept
{
	if (i.GetArgType() != DBUS_TYPE_STRING)
		return nullptr;

	return i.GetString();
}

template<typename I>
gcc_pure
static const char *
CheckVariantString(I &&i) noexcept
{
	if (i.GetArgType() != DBUS_TYPE_VARIANT)
		return nullptr;

	return CheckString(i.Recurse());
}

static void
ParseDriveDictEntry(UdisksObject &o, ODBus::ReadMessageIter &&i) noexcept
{
	if (i.GetArgType() != DBUS_TYPE_STRING)
		return;

	const char *name = i.GetString();
	i.Next();

	if (StringIsEqual(name, "Id")) {
		const char *value = CheckVariantString(i);
		if (value != nullptr && o.drive_id.empty())
			o.drive_id = value;
	}
}

static void
ParseBlockDictEntry(UdisksObject &o, ODBus::ReadMessageIter &&i) noexcept
{
	if (i.GetArgType() != DBUS_TYPE_STRING)
		return;

	const char *name = i.GetString();
	i.Next();

	if (StringIsEqual(name, "Id")) {
		const char *value = CheckVariantString(i);
		if (value != nullptr && o.block_id.empty())
			o.block_id = value;
	}
}

static void
ParseInterface(UdisksObject &o, const char *interface,
	       ODBus::ReadMessageIter &&i) noexcept
{
	if (StringIsEqual(interface, "org.freedesktop.UDisks2.Drive")) {
		for (; i.GetArgType() == DBUS_TYPE_DICT_ENTRY; i.Next())
			ParseDriveDictEntry(o, i.Recurse());
	} else if (StringIsEqual(interface, "org.freedesktop.UDisks2.Block")) {
		for (; i.GetArgType() == DBUS_TYPE_DICT_ENTRY; i.Next())
			ParseBlockDictEntry(o, i.Recurse());
	} else if (StringIsEqual(interface, "org.freedesktop.UDisks2.Filesystem")) {
		o.is_filesystem = true;
	}
}

static void
ParseInterfaceDictEntry(UdisksObject &o, ODBus::ReadMessageIter &&i) noexcept
{
	if (i.GetArgType() != DBUS_TYPE_STRING)
		return;

	const char *interface = i.GetString();
	i.Next();

	if (i.GetArgType() != DBUS_TYPE_ARRAY)
		return;

	ParseInterface(o, interface, i.Recurse());
}

static bool
ParseObject(UdisksObject &o, ODBus::ReadMessageIter &&i) noexcept
{
	if (i.GetArgType() != DBUS_TYPE_OBJECT_PATH)
		return false;

	o.path = i.GetString();

	i.Next();

	if (i.GetArgType() != DBUS_TYPE_ARRAY)
		return false;

	i.Recurse().ForEach(DBUS_TYPE_DICT_ENTRY, [&o](auto &&j){
			ParseInterfaceDictEntry(o, j.Recurse());
		});

	return true;
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
UdisksNeighborExplorer::Insert(UdisksObject &&o) noexcept
{
	assert(o.IsValid());

	const NeighborInfo info = o.ToNeighborInfo();

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
UdisksNeighborExplorer::OnListNotify(DBusPendingCall *pending) noexcept
{
	assert(pending == pending_list_call.Get());

	pending_list_call = {};

	using namespace ODBus;
	Message reply = Message::StealReply(*pending);

	try {
		reply.CheckThrowError();
	} catch (...) {
		LogError(std::current_exception());
		return;
	}

	ReadMessageIter i(*reply.Get());
	if (i.GetArgType() != DBUS_TYPE_ARRAY) {
		LogError(udisks_domain, "Malformed response");
		return;
	}

	i.Recurse().ForEach(DBUS_TYPE_DICT_ENTRY, [this](auto &&j){
			UdisksObject o;
			if (ParseObject(o, j.Recurse()) && o.IsValid())
				Insert(std::move(o));
		});
}

inline DBusHandlerResult
UdisksNeighborExplorer::HandleMessage(DBusConnection *, DBusMessage *message) noexcept
{
	using namespace ODBus;

	if (dbus_message_is_signal(message, DBUS_OM_INTERFACE,
				   "InterfacesAdded") &&
	    dbus_message_has_signature(message, DBUS_OM_INTERFACES_ADDED_SIGNATURE)) {
		UdisksObject o;
		if (ParseObject(o, ReadMessageIter(*message)) && o.IsValid())
			Insert(std::move(o));

		return DBUS_HANDLER_RESULT_HANDLED;
	} else if (dbus_message_is_signal(message, DBUS_OM_INTERFACE,
					  "InterfacesRemoved") &&
		   dbus_message_has_signature(message, DBUS_OM_INTERFACES_REMOVED_SIGNATURE)) {
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
