// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include "IntrusiveList.hxx"

/**
 * A variant of #IntrusiveList which is sorted automatically.  There
 * are obvious scalability problems with this approach, so use with
 * care.
 */
template<typename T, typename Compare=typename T::Compare,
	 typename HookTraits=IntrusiveListBaseHookTraits<T>,
	 bool constant_time_size=false>
class IntrusiveSortedList
	: public IntrusiveList<T, HookTraits, constant_time_size>
{
	using Base = IntrusiveList<T, HookTraits, constant_time_size>;

	[[no_unique_address]]
	Compare compare;

public:
	constexpr IntrusiveSortedList() noexcept = default;
	IntrusiveSortedList(IntrusiveSortedList &&src) noexcept = default;

	using typename Base::reference;
	using Base::begin;
	using Base::end;

	void insert(reference item) noexcept {
		auto position = std::find_if(begin(), end(), [this, &item](const auto &other){
			return !compare(other, item);
		});

		Base::insert(position, item);
	}
};
