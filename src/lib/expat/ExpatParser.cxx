/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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

#include "config.h"
#include "ExpatParser.hxx"
#include "input/InputStream.hxx"
#include "util/ASCII.hxx"
#include "util/Error.hxx"
#include "util/Domain.hxx"

#include <string.h>

static constexpr Domain expat_domain("expat");

void
ExpatParser::SetError(Error &error)
{
	XML_Error code = XML_GetErrorCode(parser);
	error.Format(expat_domain, int(code), "XML parser failed: %s",
		     XML_ErrorString(code));
}

bool
ExpatParser::Parse(const char *data, size_t length, bool is_final,
		   Error &error)
{
	bool success = XML_Parse(parser, data, length,
				 is_final) == XML_STATUS_OK;
	if (!success)
		SetError(error);

	return success;
}

bool
ExpatParser::Parse(InputStream &is, Error &error)
{
	assert(is.IsReady());

	while (true) {
		char buffer[4096];
		size_t nbytes = is.LockRead(buffer, sizeof(buffer), error);
		if (nbytes == 0)
			break;

		if (!Parse(buffer, nbytes, false, error))
			return false;
	}

	if (error.IsDefined())
		return false;

	return Parse("", 0, true, error);
}

const char *
ExpatParser::GetAttribute(const XML_Char **atts,
			  const char *name)
{
	for (unsigned i = 0; atts[i] != nullptr; i += 2)
		if (strcmp(atts[i], name) == 0)
			return atts[i + 1];

	return nullptr;
}

const char *
ExpatParser::GetAttributeCase(const XML_Char **atts,
			      const char *name)
{
	for (unsigned i = 0; atts[i] != nullptr; i += 2)
		if (StringEqualsCaseASCII(atts[i], name))
			return atts[i + 1];

	return nullptr;
}
