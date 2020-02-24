/*
 * Copyright 2018 Goldhorn
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

#include "Compiler.h"
#include "external/jaijson/jaijson.hxx"

#include <string>

struct QobuzEvent {
	std::string user_id;
	uint64_t date;
	uint64_t duration = 0;
	bool online = true;
	std::string intent = "streaming";
	bool sample = false;
	std::string device_id;
	std::string track_id;
	bool purchase = false;
	bool local = false;
	std::string credential_id;
	int format_id = 5;
};

void serialize(jaijson::Writer &w, const QobuzEvent &m);
