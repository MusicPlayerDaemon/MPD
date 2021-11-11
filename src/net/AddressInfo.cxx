/*
 * Copyright 2016-2021 Max Kellermann <max.kellermann@gmail.com>
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

#include "AddressInfo.hxx"
#include "Features.hxx"

#include <array>
#include <cassert>

static constexpr auto address_family_ranking = std::array {
#ifdef HAVE_UN
	AF_LOCAL,
#endif
	AF_INET6,
};

static bool
IsAddressFamilyBetter(int previous, int next)
{
	for (auto i : address_family_ranking) {
		if (next == i)
			return previous != i;
		if (previous == i)
			return false;
	}

	return false;
}

static bool
IsBetter(const AddressInfo &previous, const AddressInfo &next)
{
	return IsAddressFamilyBetter(previous.GetFamily(),
				     next.GetFamily());
}

static bool
IsBetter(const AddressInfo *previous, const AddressInfo &next)
{
	return previous == nullptr || IsBetter(*previous, next);
}

const AddressInfo &
AddressInfoList::GetBest() const
{
	assert(!empty());

	const AddressInfo *best = nullptr;

	for (const auto &i : *this)
		if (IsBetter(best, i))
			best = &i;

	assert(best != nullptr);
	return *best;
}
