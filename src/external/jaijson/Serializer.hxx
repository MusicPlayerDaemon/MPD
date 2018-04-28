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

#include <stdint.h>
#include <string>
#include <vector>
#include <list>
#include <map>
#include <unordered_map>
#include <utility>
#include <sstream>

static inline void serialize(jaijson::Writer &w, bool val) { w.Bool(val); }
static inline void serialize(jaijson::Writer &w, uint8_t val) { w.Uint(val); }
static inline void serialize(jaijson::Writer &w, uint16_t val) { w.Uint(val); }
static inline void serialize(jaijson::Writer &w, uint32_t val) { w.Uint(val); }
static inline void serialize(jaijson::Writer &w, uint64_t val) { w.Uint64(val); }
static inline void serialize(jaijson::Writer &w, int8_t val) { w.Int(val); }
static inline void serialize(jaijson::Writer &w, int16_t val) { w.Int(val); }
static inline void serialize(jaijson::Writer &w, int32_t val) { w.Int(val); }
static inline void serialize(jaijson::Writer &w, int64_t val) { w.Int64(val); }
static inline void serialize(jaijson::Writer &w, float val) { w.Double(val); }
static inline void serialize(jaijson::Writer &w, double val) { w.Double(val); }
static inline void serialize(jaijson::Writer &w, const char *val) {
	if (val == nullptr)
		w.Null();
	else
		w.String(val);
}
static inline void serialize(jaijson::Writer &w, const std::string &val) { w.String(val.c_str()); }

/*
 *  serialize enum
 */
template<typename T>
static inline void
serialize(jaijson::Writer &w, const char *key, T &value, std::vector<const char*> table) {
	w.String(key);
	w.String(table.at(unsigned(value)));
}

template<typename T>
extern inline void
serialize(jaijson::Writer &w, const char *key, const T &val);
template<typename T>
extern inline void
serialize(jaijson::Writer &w, const std::string &key, const T &val);

/*
 *  serialize pair
 */
template<typename T>
static void serialize(jaijson::Writer &w, const std::pair<std::string, T> &pair) {
	w.StartObject();
	serialize(w, pair.first, pair.second);
	w.EndObject();
}

/*
 *  serialize list
 */
template<typename T>
static void serialize(jaijson::Writer &w, const std::list<T> &list) {
	w.StartArray();
	for (const auto &item : list) {
		serialize(w, item);
	}
	w.EndArray();
}

/*
 *  serialize vector
 */
template<typename T>
static void serialize(jaijson::Writer &w, const std::vector<T> &list) {
	w.StartArray();
	for (const auto &item : list) {
		serialize(w, item);
	}
	w.EndArray();
}

/*
 *  serialize map
 */
template<typename T>
static void serialize(jaijson::Writer &w, const std::map<std::string, T> &list) {
	w.StartArray();
	for (const auto &item : list) {
		w.StartObject();
		serialize(w, item.first, item.second);
		w.EndObject();
	}
	w.EndArray();
}

/*
 *  serialize unordered_map
 */
template<typename T>
static void serialize(jaijson::Writer &w, const std::unordered_map<std::string, T> &list) {
	w.StartArray();
	for (const auto &item : list) {
		w.StartObject();
		serialize(w, item.first, item.second);
		w.EndObject();
	}
	w.EndArray();
}

template<typename T>
static inline void
serialize(jaijson::Writer &w, const char *key, const T &val)
{
	w.String(key); serialize(w, val);
}
template<typename T>
static inline void
serialize(jaijson::Writer &w, const std::string &key, const T &val)
{
	w.String(key.c_str()); serialize(w, val);
}

template<typename T>
static std::string
to_string(T t)
{
	std::stringstream stream;
	stream << t;
	return stream.str();
}

template<typename T>
static std::string
str(const T &t)
{
	jaijson::StringBuffer sb;
	jaijson::Writer w;
	w.Reset(sb);
	serialize(w, t);
	return sb.GetString();
}
