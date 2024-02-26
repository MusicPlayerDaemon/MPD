// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "util/TemplateString.hxx"

#include <dbus/dbus.h>

namespace ODBus {

template<int type>
struct BasicTypeTraits {
	static constexpr int TYPE = type;
	static constexpr auto as_string = TemplateString::FromChar(TYPE);
};

template<typename T>
struct TypeTraits {
};

template<>
struct TypeTraits<const char *> : BasicTypeTraits<DBUS_TYPE_STRING> {
};

using StringTypeTraits = TypeTraits<const char *>;

struct ObjectPathTypeTraits : BasicTypeTraits<DBUS_TYPE_OBJECT_PATH> {
};

template<>
struct TypeTraits<dbus_uint32_t> : BasicTypeTraits<DBUS_TYPE_UINT32> {
};

template<>
struct TypeTraits<dbus_uint64_t> : BasicTypeTraits<DBUS_TYPE_UINT64> {
};

using BooleanTypeTraits = BasicTypeTraits<DBUS_TYPE_BOOLEAN>;

template<typename T>
struct ArrayTypeTraits {
	using ContainedTraits = T;

	static constexpr int TYPE = DBUS_TYPE_ARRAY;
	static constexpr auto as_string =
		TemplateString::Concat(TemplateString::FromChar(TYPE),
				       ContainedTraits::as_string);
};

template<typename KeyT, typename ValueT>
struct DictEntryTypeTraits {
	static constexpr int TYPE = DBUS_TYPE_DICT_ENTRY;

	static constexpr auto as_string =
		TemplateString::Concat(TemplateString::FromChar(DBUS_DICT_ENTRY_BEGIN_CHAR),
				       KeyT::as_string,
				       ValueT::as_string,
				       TemplateString::FromChar(DBUS_DICT_ENTRY_END_CHAR));
};

using VariantTypeTraits = BasicTypeTraits<DBUS_TYPE_VARIANT>;

/**
 * Concatenate all TypeAsString members to one string.
 */
template<typename T, typename... ContainedTraits>
struct ConcatTypeAsString {
	static constexpr auto as_string =
		TemplateString::Concat(T::as_string,
				       ConcatTypeAsString<ContainedTraits...>::as_string);
};

template<typename T>
struct ConcatTypeAsString<T> {
	static constexpr auto as_string = T::as_string;
};

template<typename... ContainedTraits>
struct StructTypeTraits {
	static constexpr int TYPE = DBUS_TYPE_STRUCT;

	static constexpr auto as_string =
		TemplateString::Concat(TemplateString::FromChar(DBUS_STRUCT_BEGIN_CHAR),
				       ConcatTypeAsString<ContainedTraits...>::as_string,
				       TemplateString::FromChar(DBUS_STRUCT_END_CHAR));
};

} /* namespace ODBus */
