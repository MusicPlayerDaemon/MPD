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

#ifndef ODBUS_OBJECT_MANAGER_HXX
#define ODBUS_OBJECT_MANAGER_HXX

#include "ReadIter.hxx"

#include <dbus/dbus.h>

#define DBUS_OM_INTERFACE "org.freedesktop.DBus.ObjectManager"

#define DBUS_OM_PROPERTIES_SIGNATURE \
	DBUS_TYPE_ARRAY_AS_STRING \
	DBUS_DICT_ENTRY_BEGIN_CHAR_AS_STRING \
	DBUS_TYPE_STRING_AS_STRING \
	DBUS_TYPE_VARIANT_AS_STRING \
	DBUS_DICT_ENTRY_END_CHAR_AS_STRING

#define DBUS_OM_INTERFACES_SIGNATURE \
	DBUS_TYPE_ARRAY_AS_STRING \
	DBUS_DICT_ENTRY_BEGIN_CHAR_AS_STRING \
	DBUS_TYPE_STRING_AS_STRING \
	DBUS_OM_PROPERTIES_SIGNATURE \
	DBUS_DICT_ENTRY_END_CHAR_AS_STRING

#define DBUS_OM_INTERFACES_ADDED_SIGNATURE \
	DBUS_TYPE_OBJECT_PATH_AS_STRING \
	DBUS_OM_INTERFACES_SIGNATURE

#define DBUS_OM_INTERFACES_REMOVED_SIGNATURE \
	DBUS_TYPE_OBJECT_PATH_AS_STRING \
	DBUS_TYPE_ARRAY_AS_STRING \
	DBUS_TYPE_STRING_AS_STRING

namespace ODBus {

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
	i.ForEach(DBUS_TYPE_DICT_ENTRY, [&f](auto &&j){
			RecurseInterfaceDictEntry(j.Recurse(), f);
		});
}

}

#endif
