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

#include "ClientTraits.hxx"

#include "config/Param.hxx"
#include "config/Data.hxx"
#include "config/Option.hxx"
#include "util/RuntimeError.hxx"

#include <map>

/*static*/ const char* const ClientTraits::COMMAND_SYNTAX = "traits [list|set <name> <value>|get <name>]";

static const char* s_trait_names[] = {
	[ClientTraits::Trait::ListsSortType] = "ListsSortType"
};

using TraitsMap = std::map<ClientTraits::Trait, const char*>;
static TraitsMap s_traits_map = {

	{ ClientTraits::Trait::ListsSortType, 
		s_trait_names[ClientTraits::Trait::ListsSortType] }
};

using TraitNamesMap = std::map<std::string_view, ClientTraits::Trait>;
static TraitNamesMap s_trait_names_map = {

	{ s_trait_names[ClientTraits::Trait::ListsSortType], 
		ClientTraits::Trait::ListsSortType }
};

static const char *const s_lists_sort_type_names[] = {
	[ClientTraits::ListsSortTypeValue::CaseSensitive] = "CaseSensitive",
	[ClientTraits::ListsSortTypeValue::CaseInsensitive] = "CaseInsensitive"
};

using ListSortTypeMap = std::map<ClientTraits::ListsSortTypeValue, const char*>;
static ListSortTypeMap s_lists_sort_type_map = {

	{ ClientTraits::ListsSortTypeValue::CaseSensitive, 
		s_lists_sort_type_names[ClientTraits::ListsSortTypeValue::CaseSensitive] },

	{ ClientTraits::ListsSortTypeValue::CaseInsensitive, 
		s_lists_sort_type_names[ClientTraits::ListsSortTypeValue::CaseInsensitive] }
};

using ListSortTypeNamesMap = std::map<std::string_view, ClientTraits::ListsSortTypeValue>;
ListSortTypeNamesMap s_lists_sort_type_names_map = {

	{ s_lists_sort_type_names[ClientTraits::ListsSortTypeValue::CaseSensitive], 
		ClientTraits::ListsSortTypeValue::CaseSensitive },

	{ s_lists_sort_type_names[ClientTraits::ListsSortTypeValue::CaseInsensitive], 
		ClientTraits::ListsSortTypeValue::CaseInsensitive }
};

/**
 * defaults
 */ 
static ClientTraits::ListsSortTypeValue 
s_def_lists_sort_type = ClientTraits::ListsSortTypeValue::Default;

ClientTraits::ClientTraits() {
	m_lists_sort_type = s_def_lists_sort_type;
}

/**
 * store defaults from config
 */
/*static*/ void ClientTraits::configure(const ConfigData& config)
{
	const ConfigBlock* conf_block = config.GetBlock(ConfigBlockOption::TRAITS);
	if (conf_block == nullptr) return;

	conf_block->SetUsed();

	Trait trait = ClientTraits::Trait::ListsSortType;
	const char* trait_name = s_trait_names[trait];
	const char* trait_conf = conf_block->GetBlockValue(trait_name,
				s_lists_sort_type_map[ClientTraits::ListsSortTypeValue::Default]);

	ListSortTypeNamesMap::const_iterator
	i = s_lists_sort_type_names_map.find(trait_conf);
	if (i == s_lists_sort_type_names_map.end()) {
		throw FormatRuntimeError(
			"Invalid configuration trait value \"%s\" for trait %s (traits@%d)",
			trait_conf, trait_name, conf_block->line);
	}
	else {
		s_def_lists_sort_type = i->second;
	}
}

/*static*/ ClientTraits::Trait ClientTraits::trait(const char* name) noexcept
{
	TraitNamesMap::const_iterator i = s_trait_names_map.find(name);
	return i == s_trait_names_map.end() ? Trait::Invalid : i->second;
}

/*static*/ const char* ClientTraits::trait_name(Trait trait) noexcept {
	const TraitsMap::const_iterator i = s_traits_map.find(trait);
	return i == s_traits_map.end() ? nullptr : i->second;
}

static const char* 
lists_sort_type_name(ClientTraits::ListsSortTypeValue value) noexcept
{
	const ListSortTypeMap::const_iterator i = s_lists_sort_type_map.find(value);
	return i == s_lists_sort_type_map.end() ? nullptr : i->second;
}

const char* ClientTraits::trait_value(Trait trait) const noexcept
{
	switch (trait) {
		case Invalid:
		case TraitsEnd:
			break;
		case ListsSortType:
			return lists_sort_type_name(m_lists_sort_type);
	}
	return nullptr;
}

bool ClientTraits::set_trait(Trait trait, const char* trait_value) noexcept
{
	switch(trait) {
		case Invalid:
		case TraitsEnd:
			break;
		case ListsSortType:
			ListSortTypeNamesMap::const_iterator 
			i = s_lists_sort_type_names_map.find(std::string_view(trait_value));
			if (i != s_lists_sort_type_names_map.end()) {
				set_lists_sort_type(i->second);
				return true;
			}
	}
	return false;
}