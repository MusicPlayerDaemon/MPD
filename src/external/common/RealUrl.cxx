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

#include "config.h"
#include "RealUrl.hxx"
#include "external/jaijson/Deserializer.hxx"
#include "external/jaijson/Serializer.hxx"

bool
deserialize(const jaijson::Value &d, RealUrl &m)
{
	deserialize(d, "url", m.url);
	std::vector<std::string> urls;
	deserialize(d, "urls", urls);
	if (!urls.empty()) {
		m.url = urls.front();
	}

	return !m.url.empty();
}
