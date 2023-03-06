// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "Iter.hxx"

#include <span>

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
	std::span<const T> GetFixedArray() noexcept {
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
