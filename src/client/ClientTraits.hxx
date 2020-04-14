/*
 * Copyright 2003-2020 The Music Player Daemon Project
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

#ifndef MPD_CLIENT_TRAITS_HXX
#define MPD_CLIENT_TRAITS_HXX

#include <array>
#include <cstdint>

class ConfigData;

/**
 * Client traits
 * 
 * @see commmand traits
 */
class ClientTraits {
public:
	ClientTraits();
	~ClientTraits() = default;
	
	enum Trait: std::int8_t {
		Invalid = -1,
		TraitsBegin = 0,

		ListsSortType = TraitsBegin,

		TraitsEnd
	};

	static void configure(const ConfigData &config);

	static constexpr std::array<Trait, TraitsEnd>
	s_all_traits = {
		ListsSortType
	};

	static Trait trait(const char* name) noexcept;
	static const char* trait_name(Trait trait) noexcept;

	const char* trait_value(Trait trait) const noexcept;
	bool set_trait(Trait trait, const char* trait_value) noexcept;

	enum ListsSortTypeValue: std::int8_t {
		CaseSensitive,
		CaseInsensitive,
		Default = CaseSensitive
	};

	void set_lists_sort_type(ListsSortTypeValue listsSortType) noexcept {
		m_lists_sort_type = listsSortType;
	}

	ListsSortTypeValue get_lists_sort_type() const noexcept {
		return m_lists_sort_type;
	}

	// used by command error messages
	static const char* const COMMAND_SYNTAX;
	
private:

	ListsSortTypeValue m_lists_sort_type;
};

#endif /* MPD_CLIENT_TRAITS_HXX */

