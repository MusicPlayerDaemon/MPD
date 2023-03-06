// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_NFS_CANCELLABLE_HXX
#define MPD_NFS_CANCELLABLE_HXX

#include "util/IntrusiveList.hxx"

#include <algorithm>
#include <cassert>

template<typename T>
class CancellablePointer
	: public IntrusiveListHook<>
{
public:
	typedef T *pointer;
	typedef T &reference;
	typedef const T &const_reference;

private:
	pointer p;

public:
	explicit CancellablePointer(reference _p):p(&_p) {}

	CancellablePointer(const CancellablePointer &) = delete;

	constexpr bool IsCancelled() const {
		return p == nullptr;
	}

	void Cancel() {
		assert(!IsCancelled());

		p = nullptr;
	}

	reference Get() {
		assert(p != nullptr);

		return *p;
	}

	constexpr bool Is(const_reference other) const {
		return p == &other;
	}
};

template<typename T, typename CT=CancellablePointer<T>>
class CancellableList {
public:
	typedef typename CT::reference reference;
	typedef typename CT::const_reference const_reference;

private:
	using List = IntrusiveList<CT>;
	typedef typename List::iterator iterator;
	typedef typename List::const_iterator const_iterator;
	List list;

	class MatchPointer {
		const_reference p;

	public:
		explicit constexpr MatchPointer(const_reference _p)
			:p(_p) {}

		constexpr bool operator()(const CT &a) const {
			return a.Is(p);
		}
	};

	[[gnu::pure]]
	iterator Find(reference p) noexcept {
		return std::find_if(list.begin(), list.end(), MatchPointer(p));
	}

	[[gnu::pure]]
	const_iterator Find(const_reference p) const noexcept {
		return std::find_if(list.begin(), list.end(), MatchPointer(p));
	}

	[[gnu::pure]]
	iterator Find(CT &c) noexcept {
		return list.iterator_to(c);
	}

	[[gnu::pure]]
	const_iterator Find(const CT &c) const noexcept {
		return list.iterator_to(c);
	}

public:
#ifndef NDEBUG
	[[gnu::pure]]
	bool IsEmpty() const noexcept {
		return std::all_of(list.begin(), list.end(), [](const auto &c) { return c.IsCancelled(); });
	}
#endif

	[[gnu::pure]]
	bool Contains(const_reference p) const noexcept {
		return Find(p) != list.end();
	}

	template<typename... Args>
	CT &Add(reference p, Args&&... args) {
		assert(Find(p) == list.end());

		CT *c = new CT(p, std::forward<Args>(args)...);
		list.push_back(*c);
		return *c;
	}

	void Remove(CT &ct) {
		auto i = Find(ct);
		assert(i != list.end());

		list.erase(i);
		delete &ct;
	}

	void Cancel(reference p) {
		auto i = Find(p);
		assert(i != list.end());

		i->Cancel();
	}

	CT &Get(reference p) noexcept {
		auto i = Find(p);
		assert(i != list.end());

		return *i;
	}

	template<typename F>
	void ForEach(F &&f) {
		for (CT &i : list)
			f(i);
	}
};

#endif
