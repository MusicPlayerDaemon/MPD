// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef ODBUS_OBJECT_MANAGER_HXX
#define ODBUS_OBJECT_MANAGER_HXX

#include "ReadIter.hxx"
#include "Types.hxx"

#include <dbus/dbus.h>

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

template<typename F>
inline void
RecurseInterfaceDictEntry(ReadMessageIter &&i, F &&f)
{
	if (i.GetArgType() != DBUS_TYPE_OBJECT_PATH)
		return;

	const char *path = i.GetString();
	i.Next();
	if (i.GetArgType() != DBUS_TYPE_ARRAY)
		return;

	f(path, i.Recurse());
}

template<typename F>
inline void
ForEachInterface(ReadMessageIter &&i, F &&f)
{
	i.ForEachRecurse(DBUS_TYPE_DICT_ENTRY, [&f](auto &&j){
			RecurseInterfaceDictEntry(std::move(j), f);
		});
}

}

#endif
