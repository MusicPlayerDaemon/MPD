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
