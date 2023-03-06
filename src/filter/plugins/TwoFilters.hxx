// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_TWO_FILTERS_HXX
#define MPD_TWO_FILTERS_HXX

#include "filter/Filter.hxx"
#include "filter/Prepared.hxx"

#include <memory>
#include <string>

/**
 * A #Filter implementation which chains two other filters.
 */
class TwoFilters final : public Filter {
	std::unique_ptr<Filter> first, second;

public:
	template<typename F, typename S>
	TwoFilters(F &&_first, S &&_second) noexcept
		:Filter(_second->GetOutAudioFormat()),
		 first(std::forward<F>(_first)),
		 second(std::forward<S>(_second)) {}

	void Reset() noexcept override {
		first->Reset();
		second->Reset();
	}

	std::span<const std::byte> FilterPCM(std::span<const std::byte> src) override;
	std::span<const std::byte> Flush() override;
};

/**
 * Like #TwoFilters, but implements the #PreparedFilter interface.
 */
class PreparedTwoFilters final : public PreparedFilter {
	std::unique_ptr<PreparedFilter> first, second;
	std::string second_name;

public:
	template<typename F, typename S, typename N>
	PreparedTwoFilters(F &&_first, S &&_second, N &&_second_name) noexcept
		:first(std::forward<F>(_first)),
		 second(std::forward<S>(_second)),
		 second_name(std::forward<N>(_second_name)) {}

	std::unique_ptr<Filter> Open(AudioFormat &audio_format) override;
};

/**
 * Create a #PreparedTwoFilters instance, but only if both parameters
 * are not nullptr.
 */
template<typename F, typename S, typename N>
static std::unique_ptr<PreparedFilter>
ChainFilters(F &&first, S &&second, N &&second_name) noexcept
{
	if (!second)
		return std::forward<F>(first);

	if (!first)
		return std::forward<S>(second);

	return std::make_unique<PreparedTwoFilters>(std::forward<F>(first),
						    std::forward<S>(second),
						    std::forward<N>(second_name));
}

#endif
