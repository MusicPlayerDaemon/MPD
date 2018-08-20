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

#include "UDisks2.hxx"
#include "Message.hxx"
#include "ReadIter.hxx"
#include "ObjectManager.hxx"
#include "util/StringAPI.hxx"
#include "util/Compiler.h"

#include <functional>

namespace UDisks2 {

template<typename I>
gcc_pure
static const char *
CheckString(I &&i) noexcept
{
	if (i.GetArgType() != DBUS_TYPE_STRING)
		return nullptr;

	return i.GetString();
}

static void
ParseDriveDictEntry(Object &o, const char *name,
		    ODBus::ReadMessageIter &&value_i) noexcept
{
	if (StringIsEqual(name, "Id")) {
		const char *value = CheckString(value_i);
		if (value != nullptr && o.drive_id.empty())
			o.drive_id = value;
	}
}

static void
ParseBlockDictEntry(Object &o, const char *name,
		    ODBus::ReadMessageIter &&value_i) noexcept
{
	if (StringIsEqual(name, "Id")) {
		const char *value = CheckString(value_i);
		if (value != nullptr && o.block_id.empty())
			o.block_id = value;
	}
}

static void
ParseInterface(Object &o, const char *interface,
	       ODBus::ReadMessageIter &&i) noexcept
{
	using namespace std::placeholders;
	if (StringIsEqual(interface, "org.freedesktop.UDisks2.Drive")) {
		i.ForEachProperty(std::bind(ParseDriveDictEntry,
					    std::ref(o), _1, _2));
	} else if (StringIsEqual(interface, "org.freedesktop.UDisks2.Block")) {
		i.ForEachProperty(std::bind(ParseBlockDictEntry,
					    std::ref(o), _1, _2));
	} else if (StringIsEqual(interface, "org.freedesktop.UDisks2.Filesystem")) {
		o.is_filesystem = true;
	}
}

static void
ParseInterfaceDictEntry(Object &o, ODBus::ReadMessageIter &&i) noexcept
{
	if (i.GetArgType() != DBUS_TYPE_STRING)
		return;

	const char *interface = i.GetString();
	i.Next();

	if (i.GetArgType() != DBUS_TYPE_ARRAY)
		return;

	ParseInterface(o, interface, i.Recurse());
}

void
ParseObject(Object &o, ODBus::ReadMessageIter &&i) noexcept
{
	i.ForEach(DBUS_TYPE_DICT_ENTRY, [&o](auto &&j){
			ParseInterfaceDictEntry(o, j.Recurse());
		});
}

void
ParseObjects(ODBus::ReadMessageIter &&i,
	     std::function<void(Object &&o)> callback)
{
	using namespace ODBus;

	ForEachInterface(std::move(i), [&callback](const char *path, auto &&j){
			Object o(path);
			ParseObject(o, std::move(j));
			if (o.IsValid())
				callback(std::move(o));
		});
}

void
ParseObjects(ODBus::Message &reply,
	     std::function<void(Object &&o)> callback)
{
	using namespace ODBus;

	reply.CheckThrowError();

	ReadMessageIter i(*reply.Get());
	if (i.GetArgType() != DBUS_TYPE_ARRAY)
		throw std::runtime_error("Malformed response");

	ParseObjects(i.Recurse(), std::move(callback));
}

} // namespace UDisks2
