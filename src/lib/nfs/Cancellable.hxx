/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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

#ifndef MPD_NFS_CANCELLABLE_HXX
#define MPD_NFS_CANCELLABLE_HXX

#include "Compiler.h"

#include <list>
#include <algorithm>

#include <assert.h>

template<typename T>
class CancellablePointer {
public:
	typedef T *pointer_type;
	typedef T &reference_type;
	typedef const T &const_reference_type;

private:
	pointer_type p;

public:
	explicit constexpr CancellablePointer(reference_type _p):p(&_p) {}

	CancellablePointer(const CancellablePointer &) = delete;

	constexpr bool IsCancelled() const {
		return p == nullptr;
	}

	void Cancel() {
		assert(!IsCancelled());

		p = nullptr;
	}

	reference_type Get() {
		assert(p != nullptr);

		return *p;
	}

	constexpr bool Is(const_reference_type other) const {
		return p == &other;
	}
};

template<typename T, typename CT=CancellablePointer<T>>
class CancellableList {
public:
	typedef typename CT::reference_type reference_type;
	typedef typename CT::const_reference_type const_reference_type;

private:
	typedef std::list<CT> List;
	typedef typename List::iterator iterator;
	typedef typename List::const_iterator const_iterator;
	List list;

	class MatchPointer {
		const_reference_type p;

	public:
		explicit constexpr MatchPointer(const_reference_type _p)
			:p(_p) {}

		constexpr bool operator()(const CT &a) const {
			return a.Is(p);
		}
	};

	gcc_pure
	iterator Find(reference_type p) {
		return std::find_if(list.begin(), list.end(), MatchPointer(p));
	}

	gcc_pure
	const_iterator Find(const_reference_type p) const {
		return std::find_if(list.begin(), list.end(), MatchPointer(p));
	}

	class MatchReference {
		const CT &c;

	public:
		constexpr explicit MatchReference(const CT &_c):c(_c) {}

		gcc_pure
		bool operator()(const CT &a) const {
			return &a == &c;
		}
	};

	gcc_pure
	iterator Find(CT &c) {
		return std::find_if(list.begin(), list.end(),
				    MatchReference(c));
	}

	gcc_pure
	const_iterator Find(const CT &c) const {
		return std::find_if(list.begin(), list.end(),
				    MatchReference(c));
	}

public:
#ifndef NDEBUG
	gcc_pure
	bool IsEmpty() const {
		for (const auto &c : list)
			if (!c.IsCancelled())
				return false;

		return true;
	}
#endif

	gcc_pure
	bool Contains(const_reference_type p) const {
		return Find(p) != list.end();
	}

	template<typename... Args>
	CT &Add(reference_type p, Args&&... args) {
		assert(Find(p) == list.end());

		list.emplace_back(p, std::forward<Args>(args)...);
		return list.back();
	}

	void RemoveLast() {
		list.pop_back();
	}

	bool RemoveOptional(CT &ct) {
		auto i = Find(ct);
		if (i == list.end())
			return false;

		list.erase(i);
		return true;
	}

	void Remove(CT &ct) {
		auto i = Find(ct);
		assert(i != list.end());

		list.erase(i);
	}

	void Cancel(reference_type p) {
		auto i = Find(p);
		assert(i != list.end());

		i->Cancel();
	}
};

#endif
