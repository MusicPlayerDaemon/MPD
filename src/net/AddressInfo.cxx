// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "AddressInfo.hxx"
#include "net/Features.hxx" // for HAVE_UN

#include <array>
#include <cassert>

static constexpr auto address_family_ranking = std::array {
#ifdef HAVE_UN
	AF_LOCAL,
#endif
	AF_INET6,
};

static constexpr bool
IsAddressFamilyBetter(int previous, int next) noexcept
{
	for (auto i : address_family_ranking) {
		if (next == i)
			return previous != i;
		if (previous == i)
			return false;
	}

	return false;
}

static constexpr bool
IsBetter(const AddressInfo &previous, const AddressInfo &next) noexcept
{
	return IsAddressFamilyBetter(previous.GetFamily(),
				     next.GetFamily());
}

static constexpr bool
IsBetter(const AddressInfo *previous, const AddressInfo &next) noexcept
{
	return previous == nullptr || IsBetter(*previous, next);
}

const AddressInfo &
AddressInfoList::GetBest() const noexcept
{
	assert(!empty());

	const AddressInfo *best = nullptr;

	for (const auto &i : *this)
		if (IsBetter(best, i))
			best = &i;

	assert(best != nullptr);
	return *best;
}
