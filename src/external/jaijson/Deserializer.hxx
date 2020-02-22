/*
 * Copyright 2015-2018 Cary Audio
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

#pragma once

#include "jaijson.hxx"

#include <strings.h>
#include <stdint.h>
#include <string>
#include <vector>
#include <list>
#include <map>
#include <unordered_map>
#include <utility>

template<typename T>
static bool
deserialize(const jaijson::Value &d, std::list<T> &list);

template<typename T>
static bool
deserialize(const jaijson::Value &d, std::vector<T> &list);

template<typename T>
static bool
deserialize(const jaijson::Value &d, std::map<std::string, T> &map);

template<typename T>
static bool
deserialize(const jaijson::Value &d, std::unordered_map<std::string, T> &map);

static inline const jaijson::Value &
get_object(const jaijson::Value &v, const char *key, const jaijson::Value &def)
{
	const auto &value = v.FindMember(key);
	return (value != v.MemberEnd() && value->value.IsObject())
		? value->value : def;
}

static inline bool
deserialize(const jaijson::Value &d, bool &value) { value = d.GetBool(); return true; }
static inline bool
deserialize(const jaijson::Value &d, uint8_t &value) { value = d.GetUint(); return true; }
static inline bool
deserialize(const jaijson::Value &d, uint16_t &value) { value = d.GetUint(); return true; }
static inline bool
deserialize(const jaijson::Value &d, uint32_t &value) { value = d.GetUint(); return true; }
static inline bool
deserialize(const jaijson::Value &d, uint64_t &value) { value = d.GetUint64(); return true; }
static inline bool
deserialize(const jaijson::Value &d, int8_t &value) { value = d.GetInt(); return true; }
static inline bool
deserialize(const jaijson::Value &d, int16_t &value) { value = d.GetInt(); return true; }
static inline bool
deserialize(const jaijson::Value &d, int32_t &value) { value = d.GetInt(); return true; }
static inline bool
deserialize(const jaijson::Value &d, int64_t &value) { value = d.GetInt64(); return true; }
static inline bool
deserialize(const jaijson::Value &d, double &value) { value = d.GetDouble(); return true; }
static inline bool
deserialize(const jaijson::Value &d, float &value) { value = d.GetDouble(); return true; }
static inline bool
deserialize(const jaijson::Value &d, std::string &value) { value = d.GetString(); return true; }

/*
 *  deserialize enum
 */
template<typename T>
static bool
deserialize(const char *value, T &t, const std::vector<const char*> &table)
{
	for (unsigned i=0,size=table.size();i<size;i++) {
		if (strcasecmp(table.at(i), value) == 0) {
			t = (T)i;
			return true;
		}
	}

	return false;
}
template<typename T>
static inline bool
deserialize(const jaijson::Value &d, const char *key, T &value, const std::vector<const char*> &table)
{
	const auto &item = d.FindMember(key);

	if (item != d.MemberEnd()) { // maybe need check NULL?
		std::string str = item->value.GetString();
		return deserialize(str.c_str(), value, table);
	}

	return false;
}
/*
 *  deserialize enum
 */
template<typename T>
static bool
deserialize(const char *value, T &t, const char *const table[], unsigned size)
{
	for (unsigned i=0;i<size;i++) {
		if (strcasecmp(table[i], value) == 0) {
			t = (T)i;
			return true;
		}
	}

	return false;
}
template<typename T>
static inline bool
deserialize(const jaijson::Value &d, const char *key, T &value, const char *const table[], unsigned size)
{
	const auto &item = d.FindMember(key);

	if (item != d.MemberEnd()) { // maybe need check NULL?
		std::string str = item->value.GetString();
		return deserialize(str.c_str(), value, table, size);
	}

	return false;
}

/*
 *  deserialize pair
 */
template<typename T>
static inline bool
deserialize(const jaijson::Value &d, std::pair<std::string, T> &pair)
{
	for (const auto &it : d.GetObject()) {
		T t;
		if (deserialize(it.value, t)) {
			pair = std::make_pair(it.name.GetString(), std::move(t));
			return true;
		}
	}

	return false;
}

/*
 *  deserialize list
 */
template<typename T>
static inline bool
deserialize(const jaijson::Value &d, std::list<T> &list)
{
	for (const auto &item : d.GetArray()) {
		T t;
		if (deserialize(item, t)) {
			list.emplace_back(std::move(t));
		} else {
			// how to do error, throw ?
			//throw std::runtime_error("deserialize std::list<T> fail");
		}
	}

	return true;
}

/*
 *  deserialize vector
 */
template<typename T>
static inline bool
deserialize(const jaijson::Value &d, std::vector<T> &list)
{
	for (const auto &item : d.GetArray()) {
		T t;
		if (deserialize(item, t)) {
			list.emplace_back(std::move(t));
		} else {
			// how to do error, throw ?
			//throw std::runtime_error("deserialize std::vector<T> fail");
		}
	}

	return true;
}

/*
 *  deserialize map
 */
template<typename T>
static inline bool
deserialize(const jaijson::Value &d, std::map<std::string, T> &map)
{
	for (const auto &item : d.GetArray()) {
		for (const auto &it : item.GetObject()) {
			T t;
			if (deserialize(it.value, t)) {
				map[it.name.GetString()] = t;
			} else {
				// how to do error, throw ?
				//throw std::runtime_error("deserialize std::map<T> fail");
			}
		}
	}

	return true;
}

/*
 *  deserialize unordered_map
 */
template<typename T>
static inline bool
deserialize(const jaijson::Value &d, std::unordered_map<std::string, T> &map)
{
	for (const auto &item : d.GetArray()) {
		for (const auto &it : item.GetObject()) {
			T t;
			if (deserialize(it.value, t)) {
				printf("%s\n", it.name.GetString());
				map[it.name.GetString()] = t;
			} else {
				// how to do error, throw ?
				//throw std::runtime_error("deserialize std::map<T> fail");
			}
		}
	}

	return true;
}

template<typename T>
static bool
deserialize(const jaijson::Value &d, const char *key, T &value)
{
	const auto &item = d.FindMember(key);

	if (item != d.MemberEnd()) { // maybe need check NULL?
		deserialize(item->value, value);
		return true;
	}

	return false;
}
template<typename T>
static bool inline
deserialize(const jaijson::Value &d, const std::string &key, T &value)
{
	return deserialize(d, key.c_str(), value);
}
