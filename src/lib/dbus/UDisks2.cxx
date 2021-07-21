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

#include "UDisks2.hxx"
#include "Message.hxx"
#include "ReadIter.hxx"
#include "ObjectManager.hxx"
#include "util/StringAPI.hxx"
#include "util/StringView.hxx"
#include "util/Compiler.h"

#include <functional>
#include <stdexcept>

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

template<typename I>
gcc_pure
static StringView
CheckRecursedByteArrayToString(I &&i) noexcept
{
	if (i.GetArgType() != DBUS_TYPE_BYTE)
		return nullptr;

	auto value = i.template GetFixedArray<char>();
	return { value.data, value.size };
}

template<typename I>
gcc_pure
static StringView
CheckByteArrayToString(I &&i) noexcept
{
	if (i.GetArgType() != DBUS_TYPE_ARRAY)
		return nullptr;

	return CheckRecursedByteArrayToString(i.Recurse());
}

template<typename I>
gcc_pure
static StringView
CheckByteArrayArrayFrontToString(I &&i) noexcept
{
	if (i.GetArgType() != DBUS_TYPE_ARRAY)
		return nullptr;

	return CheckByteArrayToString(i.Recurse());
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
ParseFileesystemDictEntry(Object &o, const char *name,
			  ODBus::ReadMessageIter &&value_i) noexcept
{
	if (StringIsEqual(name, "MountPoints")) {
		if (!o.mount_point.empty())
			/* we already know one mount point, and we're
			   not interested in more */
			return;

		/* get the first string in the array */
		auto value = CheckByteArrayArrayFrontToString(value_i);
		if (value != nullptr)
			o.mount_point = {value.data, value.size};

		// TODO: check whether the string is a valid filesystem path
	}
}

static void
ParseInterface(Object &o, const char *interface,
	       ODBus::ReadMessageIter &&i) noexcept
{
	if (StringIsEqual(interface, "org.freedesktop.UDisks2.Drive")) {
		i.ForEachProperty([&](auto n, auto v) {
			return ParseDriveDictEntry(o, n, std::move(v));
		});
	} else if (StringIsEqual(interface, "org.freedesktop.UDisks2.Block")) {
		i.ForEachProperty([&](auto n, auto v) {
			return ParseBlockDictEntry(o, n, std::move(v));
		});
	} else if (StringIsEqual(interface, "org.freedesktop.UDisks2.Filesystem")) {
		o.is_filesystem = true;

		i.ForEachProperty([&](auto n, auto v) {
			return ParseFileesystemDictEntry(o, n, std::move(v));
		});
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
			ParseObject(o, std::forward<decltype(j)>(j));
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
