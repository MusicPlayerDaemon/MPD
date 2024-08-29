// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <utility>

#if defined(__GNUC__) && __GNUC__ >= 13
#pragma GCC diagnostic push
/* suppress -Wuninitialized; GCC is right, the "dispose" field is
   sometimes not initialized (if ptr==nullptr), but it's only swapped
   in the move operator/constructor, and that's okay, we're not going
   to use it in that case anyway */
#pragma GCC diagnostic ignored "-Wuninitialized"
#endif

/**
 * A generic object which is owned by somebody who doesn't know how to
 * dispose of it; to do this, a function pointer for disposing it is
 * provided.  Some implementations may do "delete this", but others
 * may be allocated from a custom allocator and may need different
 * ways to dispose of it.
 *
 * Unlike std::any, this class does not require the contained object
 * to be copyable; quite contrary, it is designed to adopt ownership
 * of the contained value.
 */
class DisposablePointer {
public:
	using DisposeFunction = void(*)(void *ptr) noexcept;

private:
	void *ptr = nullptr;

	DisposeFunction dispose;

public:
	DisposablePointer() = default;
	DisposablePointer(std::nullptr_t) noexcept {}

	DisposablePointer(void *_ptr, DisposeFunction _dispose) noexcept
		:ptr(_ptr), dispose(_dispose) {}

	DisposablePointer(DisposablePointer &&src) noexcept
		:ptr(std::exchange(src.ptr, nullptr)), dispose(src.dispose) {}

	~DisposablePointer() noexcept {
		if (ptr != nullptr)
			dispose(ptr);
	}

	DisposablePointer &operator=(DisposablePointer &&other) noexcept {
		using std::swap;
		swap(ptr, other.ptr);
		swap(dispose, other.dispose);
		return *this;
	}

	operator bool() const noexcept {
		return ptr != nullptr;
	}

	void *get() const noexcept {
		return ptr;
	}

	void reset() noexcept {
		if (ptr != nullptr)
			dispose(std::exchange(ptr, nullptr));
	}
};

#if defined(__GNUC__) && __GNUC__ >= 13
#pragma GCC diagnostic pop
#endif

template<typename T>
class TypedDisposablePointer : public DisposablePointer {
public:
	template<typename... Args>
	TypedDisposablePointer(Args&&... args) noexcept
		:DisposablePointer(std::forward<Args>(args)...) {}

	TypedDisposablePointer(void *_ptr, DisposeFunction _dispose) noexcept;

	TypedDisposablePointer(T *_ptr, DisposeFunction _dispose) noexcept
		:DisposablePointer(_ptr, _dispose) {}

	T *get() const noexcept {
		return (T *)DisposablePointer::get();
	}

	T *operator->() const noexcept {
		return get();
	}

	T &operator*() const noexcept {
		return *get();
	}
};

inline DisposablePointer
ToNopPointer(const void *ptr) noexcept
{
	/* since the disposer is a no-op, we allow passing a const
	   pointer here; the const_cast is necessary because
	   DisposablePointer wants a non-const pointer */
	return {const_cast<void *>(ptr), [](void *) noexcept {}};
}

template<typename T>
TypedDisposablePointer<T>
ToDeletePointer(T *ptr) noexcept
{
	return {ptr, [](void *p) noexcept {
		T *t = (T *)p;
		delete t;
	}};
}

template<typename T>
TypedDisposablePointer<T>
ToDeleteArray(T *ptr) noexcept
{
	return {ptr, [](void *p) noexcept {
		T *t = (T *)p;
		delete[] t;
	}};
}

template<typename T>
TypedDisposablePointer<T>
ToDestructPointer(T *ptr) noexcept
{
	return {ptr, [](void *p) noexcept {
		T *t = (T *)p;
		t->~T();
	}};
}
