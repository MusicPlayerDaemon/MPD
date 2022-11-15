/*
 * Copyright 2003-2022 The Music Player Daemon Project
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

#include "Canonicalize.hxx"
#include "config.h"

#ifdef HAVE_ICU_CANONICALIZE

#include "util/AllocatedString.hxx"

#ifdef HAVE_ICU
#include "Normalize.hxx"
#include "Transliterator.hxx"
#include "Util.hxx"
#include "util/AllocatedArray.hxx"
#include "util/SpanCast.hxx"
#endif

#ifdef HAVE_ICU

using std::string_view_literals::operator""sv;

static IcuTransliterator *transliterator;

void
IcuCanonicalizeInit()
{
	assert(transliterator == nullptr);

	const auto id =
		/* convert all punctuation to ASCII equivalents */
		"[:Punctuation:] Latin-ASCII;"sv;

	transliterator = new IcuTransliterator(ToStringView(std::span{UCharFromUTF8(id)}),
					       {});
}

void
IcuCanonicalizeFinish() noexcept
{
	assert(transliterator != nullptr);

	delete transliterator;
	transliterator = nullptr;
}

#endif

AllocatedString
IcuCanonicalize(std::string_view src, bool fold_case) noexcept
try {
#ifdef HAVE_ICU
	assert(transliterator != nullptr);

	auto u = UCharFromUTF8(src);
	if (u.data() == nullptr)
		return {src};

	if (auto n = fold_case
	    ? IcuNormalizeCaseFold(ToStringView(std::span{u}))
	    : IcuNormalize(ToStringView(std::span{u}));
	    n != nullptr)
		u = std::move(n);

	if (auto t = transliterator->Transliterate(ToStringView(std::span{u}));
	    t != nullptr)
		u = std::move(t);

	return UCharToUTF8(ToStringView(std::span{u}));
#else
#error not implemented
#endif
} catch (...) {
	return {src};
}

#endif /* HAVE_ICU_CANONICALIZE */
