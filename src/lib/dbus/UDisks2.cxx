// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "UDisks2.hxx"
#include "Message.hxx"
#include "ReadIter.hxx"
#include "ObjectManager.hxx"
#include "util/SpanCast.hxx"
#include "util/StringAPI.hxx"

#include <functional>
#include <stdexcept>

namespace UDisks2 {

template<typename I>
[[gnu::pure]]
static const char *
CheckString(I &&i) noexcept
{
	if (i.GetArgType() != DBUS_TYPE_STRING)
		return nullptr;

	return i.GetString();
}

template<typename I>
[[gnu::pure]]
static std::string_view
CheckRecursedByteArrayToString(I &&i) noexcept
{
	if (i.GetArgType() != DBUS_TYPE_BYTE)
		return {};

	auto value = i.template GetFixedArray<char>();
	return ToStringView(value);
}

template<typename I>
[[gnu::pure]]
static std::string_view
CheckByteArrayToString(I &&i) noexcept
{
	if (i.GetArgType() != DBUS_TYPE_ARRAY)
		return {};

	return CheckRecursedByteArrayToString(i.Recurse());
}

template<typename I>
[[gnu::pure]]
static std::string_view
CheckByteArrayArrayFrontToString(I &&i) noexcept
{
	if (i.GetArgType() != DBUS_TYPE_ARRAY)
		return {};

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
		if (value.data() != nullptr)
			o.mount_point = value;

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
