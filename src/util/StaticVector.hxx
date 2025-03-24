// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <initializer_list>
#include <new>
#include <span>
#include <type_traits>
#include <utility>

/**
 * An array with a maximum size known at compile time.  It keeps track
 * of the actual length at runtime.
 */
template<class T, std::size_t max>
class StaticVector {
	struct alignas(T) Storage {
		std::byte data[sizeof(T)];
	};

	using Array = std::array<Storage, max>;

public:
	using size_type = typename Array::size_type;
	using value_type = T;
	using reference = T &;
	using const_reference =  const T &;
	using pointer =  T *;
	using const_pointer =  const T *;
	using iterator = pointer;
	using const_iterator = const_pointer;

private:
	size_type the_size = 0;
	Array array;

	static constexpr pointer Launder(Storage *storage) noexcept {
		return std::launder(reinterpret_cast<pointer>(storage->data));
	}

	static constexpr const_pointer Launder(const Storage *storage) noexcept {
		return std::launder(reinterpret_cast<const_pointer>(storage->data));
	}

public:
	constexpr StaticVector() noexcept = default;

	constexpr StaticVector(size_type _size, const_reference value)
	{
		if (_size > max)
			throw std::bad_alloc{};

		while (_size-- > 0)
			push_back(value);
	}

	/**
	 * Initialise the array with values from the iterator range.
	 */
	template<typename I>
	constexpr StaticVector(I _begin, I _end)
		:the_size(0)
	{
		for (I i = _begin; i != _end; ++i)
			push_back(*i);
	}

	template<typename U>
	constexpr StaticVector(std::initializer_list<U> init) noexcept
		:the_size(init.size())
	{
		static_assert(init.size() <= max);

		for (auto &i : init)
			emplace_back(std::move(i));
	}

	constexpr ~StaticVector() noexcept {
		clear();
	}

	constexpr operator std::span<const T>() const noexcept {
		return {
			Launder(array.data()),
			the_size,
		};
	}

	constexpr operator std::span<T>() noexcept {
		return {
			Launder(array.data()),
			the_size,
		};
	}

	constexpr size_type max_size() const noexcept {
		return max;
	}

	constexpr size_type size() const noexcept {
		return the_size;
	}

	constexpr bool empty() const noexcept {
		return the_size == 0;
	}

	constexpr bool full() const noexcept {
		return the_size == max;
	}

	constexpr void clear() noexcept {
		if constexpr (std::is_trivially_destructible_v<T>) {
			/* we don't need to call any destructor */
			the_size = 0;
		} else {
			while (!empty())
				pop_back();
		}
	}

	/**
	 * Returns one element.  No bounds checking.
	 */
	constexpr reference operator[](size_type i) noexcept {
		assert(i < size());

		return *Launder(&array[i]);
	}

	/**
	 * Returns one constant element.  No bounds checking.
	 */
	constexpr const_reference operator[](size_type i) const noexcept {
		assert(i < size());

		return *Launder(&array[i]);
	}

	constexpr reference front() noexcept {
		assert(!empty());

		return *Launder(&array.front());
	}

	constexpr const_reference front() const noexcept {
		assert(!empty());

		return *Launder(&array.front());
	}

	constexpr reference back() noexcept {
		assert(!empty());

		return *Launder(&array[the_size - 1]);
	}

	constexpr const_reference back() const noexcept {
		assert(!empty());

		return *Launder(&array[the_size - 1]);
	}

	constexpr iterator begin() noexcept {
		return Launder(&array.front());
	}

	constexpr const_iterator begin() const noexcept {
		return Launder(&array.front());
	}

	constexpr iterator end() noexcept {
		return std::next(begin(), the_size);
	}

	constexpr const_iterator end() const noexcept {
		return std::next(begin(), the_size);
	}

	/**
	 * Return address of start of data segment.
	 */
	constexpr pointer data() noexcept {
		return Launder(array.data());
	}

	constexpr const_pointer data() const noexcept {
		return Launder(array.data());
	}

	constexpr reference push_back(const_reference value) {
		return emplace_back(value);
	}

	constexpr reference push_back(T &&value) {
		return emplace_back(std::move(value));
	}

	template<typename... Args>
	constexpr reference emplace_back(Args&&... args) {
		if (full())
			throw std::bad_alloc{};

		::new(&array[the_size]) T(std::forward<Args>(args)...);
		return *Launder(&array[the_size++]);
	}

	constexpr void pop_front() noexcept {
		assert(!empty());

		erase(begin());
	}

	constexpr void pop_back() noexcept {
		assert(!empty());

		back().~T();
		--the_size;
	}

	constexpr iterator erase(iterator first, iterator last) noexcept {
		std::size_t n = std::distance(first, last);
		std::move(last, end(), first);
		the_size -= n;
		return first;
	}

	constexpr iterator erase(iterator pos) noexcept {
		return erase(pos, std::next(pos));
	}
};
