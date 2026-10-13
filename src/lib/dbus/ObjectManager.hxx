// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include "ReadIter.hxx"
#include "Types.hxx"

#include <dbus/dbus.h>

#include <concepts>

#define DBUS_OM_INTERFACE "org.freedesktop.DBus.ObjectManager"

namespace ODBus {

using PropertiesType =
	ArrayTypeTraits<DictEntryTypeTraits<StringTypeTraits,
					    VariantTypeTraits>>;

using InterfacesType =
	ArrayTypeTraits<DictEntryTypeTraits<StringTypeTraits,
					    PropertiesType>>;

using InterfacesAddedType =
	ConcatTypeAsString<ObjectPathTypeTraits,
			   InterfacesType>;

using InterfacesRemovedType = ConcatTypeAsString<ObjectPathTypeTraits,
						 ArrayTypeTraits<StringTypeTraits>>;

inline void
RecurseInterfaceDictEntry(ReadMessageIter &&i,
			  std::invocable<const char *, ReadMessageIter> auto &&f)
{
	if (i.GetArgType() != DBUS_TYPE_OBJECT_PATH)
		return;

	const char *path = i.GetString();
	i.Next();
	if (i.GetArgType() != DBUS_TYPE_ARRAY)
		return;

	f(path, i.Recurse());
}

inline void
ForEachInterface(ReadMessageIter &&i,
		 std::invocable<const char *, ReadMessageIter> auto &&f)
{
	i.ForEachRecurse(DBUS_TYPE_DICT_ENTRY, [&f](auto &&j){
			RecurseInterfaceDictEntry(std::move(j), f);
		});
}

} // namespace ODBus
