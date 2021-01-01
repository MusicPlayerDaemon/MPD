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

#include "ExpatParser.hxx"
#include "util/ASCII.hxx"

#include <string.h>

void
ExpatParser::Parse(const char *data, size_t length, bool is_final)
{
	if (XML_Parse(parser, data, length, is_final) != XML_STATUS_OK)
		throw ExpatError(parser);
}

const char *
ExpatParser::GetAttribute(const XML_Char **atts,
			  const char *name) noexcept
{
	for (unsigned i = 0; atts[i] != nullptr; i += 2)
		if (strcmp(atts[i], name) == 0)
			return atts[i + 1];

	return nullptr;
}

const char *
ExpatParser::GetAttributeCase(const XML_Char **atts,
			      const char *name) noexcept
{
	for (unsigned i = 0; atts[i] != nullptr; i += 2)
		if (StringEqualsCaseASCII(atts[i], name))
			return atts[i + 1];

	return nullptr;
}
