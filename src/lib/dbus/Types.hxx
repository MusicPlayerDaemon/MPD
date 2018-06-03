/*
 * Copyright 2007-2017 Content Management AG
 * All rights reserved.
 *
 * author: Max Kellermann <mk@cm4all.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ODBUS_TYPES_HXX
#define ODBUS_TYPES_HXX

#include "util/TemplateString.hxx"

#include <dbus/dbus.h>

namespace ODBus {

template<int type>
struct BasicTypeTraits {
	static constexpr int TYPE = type;
	typedef TemplateString::CharAsString<TYPE> TypeAsString;
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

using BooleanTypeTraits = BasicTypeTraits<DBUS_TYPE_BOOLEAN>;

template<typename T>
struct ArrayTypeTraits {
	typedef T ContainedTraits;

	static constexpr int TYPE = DBUS_TYPE_ARRAY;
	typedef TemplateString::InsertBefore<TYPE, typename ContainedTraits::TypeAsString> TypeAsString;
};

template<typename KeyT, typename ValueT>
struct DictEntryTypeTraits {
	static constexpr int TYPE = DBUS_TYPE_DICT_ENTRY;

	typedef TemplateString::Concat<TemplateString::CharAsString<DBUS_DICT_ENTRY_BEGIN_CHAR>,
				       typename KeyT::TypeAsString,
				       typename ValueT::TypeAsString,
				       TemplateString::CharAsString<DBUS_DICT_ENTRY_END_CHAR>> TypeAsString;
};

using VariantTypeTraits = BasicTypeTraits<DBUS_TYPE_VARIANT>;

/**
 * Concatenate all TypeAsString members to one string.
 */
template<typename T, typename... ContainedTraits>
struct ConcatTypeAsString
	: TemplateString::Concat<typename T::TypeAsString,
				 ConcatTypeAsString<ContainedTraits...>> {};

template<typename T>
struct ConcatTypeAsString<T> : T::TypeAsString {};

template<typename... ContainedTraits>
struct StructTypeTraits {
	static constexpr int TYPE = DBUS_TYPE_STRUCT;

	typedef TemplateString::Concat<TemplateString::CharAsString<DBUS_STRUCT_BEGIN_CHAR>,
				       ConcatTypeAsString<ContainedTraits...>,
				       TemplateString::CharAsString<DBUS_STRUCT_END_CHAR>> TypeAsString;
};

} /* namespace ODBus */

#endif
