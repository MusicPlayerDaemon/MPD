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

#include <rapidjson/document.h>
#include <rapidjson/rapidjson.h>
#include <rapidjson/prettywriter.h>

namespace jaijson {

#define VERSION_INT(a, b, c) ((a)<<16 | (b)<<8 | (c))
#define RAPIDJSON_VERSION_INT VERSION_INT(RAPIDJSON_MAJOR_VERSION, RAPIDJSON_MINOR_VERSION, RAPIDJSON_PATCH_VERSION)

#if !defined(RAPIDJSON_MAJOR_VERSION) || RAPIDJSON_VERSION_INT < VERSION_INT(1, 0, 2)
#error "You should install rapidjson >= 1.0.2 in you include dir"
#endif

typedef rapidjson::Value Value;
typedef rapidjson::Document Document;
typedef rapidjson::Writer<rapidjson::StringBuffer> Writer;
typedef rapidjson::StringBuffer StringBuffer;

}
