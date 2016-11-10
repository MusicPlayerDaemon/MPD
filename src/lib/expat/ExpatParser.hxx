/*
 * Copyright 2003-2016 The Music Player Daemon Project
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

#ifndef MPD_EXPAT_HXX
#define MPD_EXPAT_HXX

#include "check.h"
#include "Compiler.h"

#include <expat.h>

#include <stdexcept>

class InputStream;

class ExpatError final : public std::runtime_error {
public:
	ExpatError(XML_Error code)
		:std::runtime_error(XML_ErrorString(code)) {}

	ExpatError(XML_Parser parser)
		:ExpatError(XML_GetErrorCode(parser)) {}
};

class ExpatParser final {
	const XML_Parser parser;

public:
	ExpatParser(void *userData)
		:parser(XML_ParserCreate(nullptr)) {
		XML_SetUserData(parser, userData);
	}

	~ExpatParser() {
		XML_ParserFree(parser);
	}

	ExpatParser(const ExpatParser &) = delete;
	ExpatParser &operator=(const ExpatParser &) = delete;

	void SetElementHandler(XML_StartElementHandler start,
			       XML_EndElementHandler end) {
		XML_SetElementHandler(parser, start, end);
	}

	void SetCharacterDataHandler(XML_CharacterDataHandler charhndl) {
		XML_SetCharacterDataHandler(parser, charhndl);
	}

	void Parse(const char *data, size_t length, bool is_final);

	void Parse(InputStream &is);

	gcc_pure
	static const char *GetAttribute(const XML_Char **atts,
					const char *name);

	gcc_pure
	static const char *GetAttributeCase(const XML_Char **atts,
					    const char *name);
};

/**
 * A specialization of #ExpatParser that provides the most common
 * callbacks as virtual methods.
 */
class CommonExpatParser {
	ExpatParser parser;

public:
	CommonExpatParser():parser(this) {
		parser.SetElementHandler(StartElement, EndElement);
		parser.SetCharacterDataHandler(CharacterData);
	}

	void Parse(const char *data, size_t length, bool is_final) {
		parser.Parse(data, length, is_final);
	}

	void Parse(InputStream &is) {
		parser.Parse(is);
	}

	gcc_pure
	static const char *GetAttribute(const XML_Char **atts,
					const char *name) {
		return ExpatParser::GetAttribute(atts, name);
	}

	gcc_pure
	static const char *GetAttributeCase(const XML_Char **atts,
					    const char *name) {
		return ExpatParser::GetAttributeCase(atts, name);
	}

protected:
	virtual void StartElement(const XML_Char *name,
				  const XML_Char **atts) = 0;
	virtual void EndElement(const XML_Char *name) = 0;
	virtual void CharacterData(const XML_Char *s, int len) = 0;

private:
	static void XMLCALL StartElement(void *user_data, const XML_Char *name,
					 const XML_Char **atts) {
		CommonExpatParser &p = *(CommonExpatParser *)user_data;
		p.StartElement(name, atts);
	}

	static void XMLCALL EndElement(void *user_data, const XML_Char *name) {
		CommonExpatParser &p = *(CommonExpatParser *)user_data;
		p.EndElement(name);
	}

	static void XMLCALL CharacterData(void *user_data,
					  const XML_Char *s, int len) {
		CommonExpatParser &p = *(CommonExpatParser *)user_data;
		p.CharacterData(s, len);
	}
};

#endif
