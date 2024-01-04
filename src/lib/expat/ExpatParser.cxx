// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "ExpatParser.hxx"
#include "util/ASCII.hxx"

#include <string.h>

void
ExpatParser::Parse(std::string_view src, bool is_final)
{
	if (XML_Parse(parser, src.data(), src.size(), is_final) != XML_STATUS_OK)
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
