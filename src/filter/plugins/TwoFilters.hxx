/*
 * Copyright 2003-2021 The Music Player Daemon Project
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

	ConstBuffer<void> FilterPCM(ConstBuffer<void> src) override;
	ConstBuffer<void> Flush() override;
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
