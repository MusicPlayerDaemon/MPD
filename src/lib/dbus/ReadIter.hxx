/*
 * Copyright 2007-2020 CM4all GmbH
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

#ifndef ODBUS_READ_ITER_HXX
#define ODBUS_READ_ITER_HXX

#include "Iter.hxx"
#include "util/ConstBuffer.hxx"

namespace ODBus {

class ReadMessageIter : public MessageIter {
	struct RecurseTag {};
	ReadMessageIter(RecurseTag, ReadMessageIter &parent) noexcept {
		dbus_message_iter_recurse(&parent.iter, &iter);
	}

public:
	explicit ReadMessageIter(DBusMessage &msg) noexcept {
		dbus_message_iter_init(&msg, &iter);
	}

	bool HasNext() noexcept {
		return dbus_message_iter_has_next(&iter);
	}

	bool Next() noexcept {
		return dbus_message_iter_next(&iter);
	}

	int GetArgType() noexcept {
		return dbus_message_iter_get_arg_type(&iter);
	}

	const char *GetSignature() noexcept {
		return dbus_message_iter_get_signature(&iter);
	}

	void GetBasic(void *value) noexcept {
		dbus_message_iter_get_basic(&iter, value);
	}

	const char *GetString() noexcept {
		const char *value;
		GetBasic(&value);
		return value;
	}

	template<typename T>
	ConstBuffer<T> GetFixedArray() noexcept {
		void *value;
		int n_elements;
		dbus_message_iter_get_fixed_array(&iter, &value, &n_elements);
		return {(const T *)value, size_t(n_elements)};
	}

	/**
	 * Create a new iterator which recurses into a container
	 * value.
	 */
	ReadMessageIter Recurse() noexcept {
		return {RecurseTag(), *this};
	}

	/**
	 * Invoke a function for each element (including the current
	 * one), as long as the argument type is the specified one.
	 */
	template<typename F>
	void ForEach(int arg_type, F &&f) {
		for (; GetArgType() == arg_type; Next())
			f(*this);
	}

	/**
	 * Wrapper for ForEach() which passes a recursed iterator for
	 * each element.
	 */
	template<typename F>
	void ForEachRecurse(int arg_type, F &&f) {
		ForEach(arg_type, [&f](auto &&i){
				f(i.Recurse());
			});
	}

	/**
	 * Invoke a function for each name/value pair (string/variant)
	 * in a dictionary (array containing #DBUS_TYPE_DICT_ENTRY).
	 * The function gets two parameters: the property name (as C
	 * string) and the variant value (as #ReadMessageIter).
	 */
	template<typename F>
	void ForEachProperty(F &&f) {
		ForEachRecurse(DBUS_TYPE_DICT_ENTRY, [&f](auto &&i){
				if (i.GetArgType() != DBUS_TYPE_STRING)
					return;

				const char *name = i.GetString();
				i.Next();

				if (i.GetArgType() != DBUS_TYPE_VARIANT)
					return;

				f(name, i.Recurse());
			});
	}
};

} /* namespace ODBus */

#endif
