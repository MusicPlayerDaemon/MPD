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

#include "Util.hxx"

#include <upnp/ixml.h>

#include <assert.h>

/** Get rid of white space at both ends */
void
trimstring(std::string &s, const char *ws)
{
	auto pos = s.find_first_not_of(ws);
	if (pos == std::string::npos) {
		s.clear();
		return;
	}
	s.replace(0, pos, std::string());

	pos = s.find_last_not_of(ws);
	if (pos != std::string::npos && pos != s.length()-1)
		s.replace(pos + 1, std::string::npos, std::string());
}

static void
path_catslash(std::string &s)
{
	if (s.empty() || s.back() != '/')
		s += '/';
}

std::string
path_getfather(const std::string &s)
{
	std::string father = s;

	// ??
	if (father.empty())
		return "./";

	if (father.back() == '/') {
		// Input ends with /. Strip it, handle special case for root
		if (father.length() == 1)
			return father;
		father.erase(father.length()-1);
	}

	auto slp = father.rfind('/');
	if (slp == std::string::npos)
		return "./";

	father.erase(slp);
	path_catslash(father);
	return father;
}

std::list<std::string>
stringToTokens(const std::string &str,
	       const char *delims, bool skipinit)
{
	std::list<std::string> tokens;

	std::string::size_type startPos = 0;

	// Skip initial delims, return empty if this eats all.
	if (skipinit &&
	    (startPos = str.find_first_not_of(delims, 0)) == std::string::npos)
		return tokens;

	while (startPos < str.size()) {
		// Find next delimiter or end of string (end of token)
		auto pos = str.find_first_of(delims, startPos);

		// Add token to the vector and adjust start
		if (pos == std::string::npos) {
			tokens.emplace_back(str, startPos);
			break;
		} else if (pos == startPos) {
			// Dont' push empty tokens after first
			if (tokens.empty())
				tokens.emplace_back();
			startPos = ++pos;
		} else {
			tokens.emplace_back(str, startPos, pos - startPos);
			startPos = ++pos;
		}
	}

	return tokens;
}

template <class T>
bool
csvToStrings(const char *s, T &tokens)
{
	assert(tokens.empty());

	std::string current;

	while (true) {
		char ch = *s++;
		if (ch == 0) {
			tokens.emplace_back(std::move(current));
			return true;
		}

		if (ch == '\\') {
			ch = *s++;
			if (ch == 0)
				return false;
		} else if (ch == ',') {
			tokens.emplace_back(std::move(current));
			current.clear();
			continue;
		}

		current.push_back(ch);
	}
}

template bool csvToStrings<std::list<std::string>>(const char *, std::list<std::string> &);
