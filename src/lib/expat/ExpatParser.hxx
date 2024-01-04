// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include <expat.h>

#include <stdexcept>
#include <string_view>
#include <utility>

class InputStream;

class ExpatError final : public std::runtime_error {
public:
	explicit ExpatError(XML_Error code)
		:std::runtime_error(XML_ErrorString(code)) {}

	explicit ExpatError(XML_Parser parser)
		:ExpatError(XML_GetErrorCode(parser)) {}
};

struct ExpatNamespaceSeparator {
	char separator;
};

class ExpatParser final {
	const XML_Parser parser;

public:
	explicit ExpatParser(void *userData)
		:parser(XML_ParserCreate(nullptr)) {
		XML_SetUserData(parser, userData);
	}

	ExpatParser(ExpatNamespaceSeparator ns, void *userData)
		:parser(XML_ParserCreateNS(nullptr, ns.separator)) {
		XML_SetUserData(parser, userData);
	}

	~ExpatParser() {
		XML_ParserFree(parser);
	}

	ExpatParser(const ExpatParser &) = delete;
	ExpatParser &operator=(const ExpatParser &) = delete;

	void SetElementHandler(XML_StartElementHandler start,
			       XML_EndElementHandler end) noexcept {
		XML_SetElementHandler(parser, start, end);
	}

	void SetCharacterDataHandler(XML_CharacterDataHandler charhndl) noexcept {
		XML_SetCharacterDataHandler(parser, charhndl);
	}

	void Parse(std::string_view src, bool is_final=false);

	void CompleteParse() {
		Parse({}, true);
	}

	void Parse(InputStream &is);

	[[gnu::pure]]
	static const char *GetAttribute(const XML_Char **atts,
					const char *name) noexcept;

	[[gnu::pure]]
	static const char *GetAttributeCase(const XML_Char **atts,
					    const char *name) noexcept;
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

	explicit CommonExpatParser(ExpatNamespaceSeparator ns)
		:parser(ns, this) {
		parser.SetElementHandler(StartElement, EndElement);
		parser.SetCharacterDataHandler(CharacterData);
	}

	template<typename... Args>
	void Parse(Args&&... args) {
		parser.Parse(std::forward<Args>(args)...);
	}

	void CompleteParse() {
		parser.CompleteParse();
	}

	[[gnu::pure]]
	static const char *GetAttribute(const XML_Char **atts,
					const char *name) noexcept {
		return ExpatParser::GetAttribute(atts, name);
	}

	[[gnu::pure]]
	static const char *GetAttributeCase(const XML_Char **atts,
					    const char *name) noexcept {
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
