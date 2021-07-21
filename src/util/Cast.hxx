/*
 * Copyright (C) 2013-2014 Max Kellermann <max.kellermann@gmail.com>
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

#ifndef CAST_HXX
#define CAST_HXX

#include "OffsetPointer.hxx"

#include <cstddef>

template<typename T, typename U>
constexpr T *
OffsetCast(U *p, std::ptrdiff_t offset)
{
	return reinterpret_cast<T *>(OffsetPointer(p, offset));
}

template<typename T, typename U>
constexpr T *
OffsetCast(const U *p, std::ptrdiff_t offset)
{
	return reinterpret_cast<const T *>(OffsetPointer(p, offset));
}

template<class C, class A>
constexpr std::ptrdiff_t
ContainerAttributeOffset(const C *null_c, const A C::*p)
{
	return std::ptrdiff_t((const char *)&(null_c->*p) - (const char *)null_c);
}

template<class C, class A>
constexpr std::ptrdiff_t
ContainerAttributeOffset(const A C::*p)
{
	return ContainerAttributeOffset<C, A>(nullptr, p);
}

/**
 * Cast the given pointer to a struct member to its parent structure.
 */
template<class C, class A>
constexpr C &
ContainerCast(A &a, const A C::*member)
{
	return *OffsetCast<C, A>(&a, -ContainerAttributeOffset<C, A>(member));
}

/**
 * Cast the given pointer to a struct member to its parent structure.
 */
template<class C, class A>
constexpr const C &
ContainerCast(const A &a, const A C::*member)
{
	return *OffsetCast<const C, const A>(&a, -ContainerAttributeOffset<C, A>(member));
}

#endif
