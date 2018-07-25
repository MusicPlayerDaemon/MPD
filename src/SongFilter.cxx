/*
 * Copyright 2003-2017 The Music Player Daemon Project
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
#include "SongFilter.hxx"
#include "db/LightSong.hxx"
#include "tag/ParseName.hxx"
#include "tag/Tag.hxx"
#include "util/CharUtil.hxx"
#include "util/ChronoUtil.hxx"
#include "util/ConstBuffer.hxx"
#include "util/RuntimeError.hxx"
#include "util/StringAPI.hxx"
#include "util/StringCompare.hxx"
#include "util/StringStrip.hxx"
#include "util/StringView.hxx"
#include "util/ASCII.hxx"
#include "util/TimeISO8601.hxx"
#include "util/UriUtil.hxx"
#include "lib/icu/CaseFold.hxx"

#include <exception>

#include <assert.h>
#include <stdlib.h>

#define LOCATE_TAG_FILE_KEY     "file"
#define LOCATE_TAG_FILE_KEY_OLD "filename"
#define LOCATE_TAG_ANY_KEY      "any"

unsigned
locate_parse_type(const char *str) noexcept
{
	if (StringEqualsCaseASCII(str, LOCATE_TAG_FILE_KEY) ||
	    StringEqualsCaseASCII(str, LOCATE_TAG_FILE_KEY_OLD))
		return LOCATE_TAG_FILE_TYPE;

	if (StringEqualsCaseASCII(str, LOCATE_TAG_ANY_KEY))
		return LOCATE_TAG_ANY_TYPE;

	if (strcmp(str, "base") == 0)
		return LOCATE_TAG_BASE_TYPE;

	if (strcmp(str, "modified-since") == 0)
		return LOCATE_TAG_MODIFIED_SINCE;

	return tag_name_parse_i(str);
}

SongFilter::Item::Item(unsigned _tag, std::string &&_value, bool _fold_case)
	:tag(_tag),
	 value(std::move(_value)),
	 fold_case(_fold_case ? IcuCompare(value.c_str()) : IcuCompare())
{
}

SongFilter::Item::Item(unsigned _tag,
		       std::chrono::system_clock::time_point _time)
	:tag(_tag), time(_time)
{
}

std::string
SongFilter::Item::ToExpression() const noexcept
{
	switch (tag) {
	case LOCATE_TAG_FILE_TYPE:
		return std::string("(" LOCATE_TAG_FILE_KEY " ") + (IsNegated() ? "!=" : "==") + " \"" + value + "\")";

	case LOCATE_TAG_BASE_TYPE:
		return "(base \"" + value + "\")";

	case LOCATE_TAG_MODIFIED_SINCE:
		return "(modified-since \"" + value + "\")";

	case LOCATE_TAG_ANY_TYPE:
		return std::string("(" LOCATE_TAG_ANY_KEY " ") + (IsNegated() ? "!=" : "==") + " \"" + value + "\")";

	default:
		return std::string("(") + tag_item_names[tag] + " " + (IsNegated() ? "!=" : "==") + " \"" + value + "\")";
	}
}

bool
SongFilter::Item::StringMatchNN(const char *s) const noexcept
{
#if !CLANG_CHECK_VERSION(3,6)
	/* disabled on clang due to -Wtautological-pointer-compare */
	assert(s != nullptr);
#endif

	assert(tag != LOCATE_TAG_MODIFIED_SINCE);

	if (fold_case) {
		return fold_case.IsIn(s);
	} else {
		return StringIsEqual(s, value.c_str());
	}
}

bool
SongFilter::Item::MatchNN(const TagItem &item) const noexcept
{
	return (tag == LOCATE_TAG_ANY_TYPE || (unsigned)item.type == tag) &&
		StringMatchNN(item.value);
}

bool
SongFilter::Item::MatchNN(const Tag &_tag) const noexcept
{
	bool visited_types[TAG_NUM_OF_ITEM_TYPES];
	std::fill_n(visited_types, size_t(TAG_NUM_OF_ITEM_TYPES), false);

	for (const auto &i : _tag) {
		visited_types[i.type] = true;

		if (MatchNN(i))
			return true;
	}

	if (tag < TAG_NUM_OF_ITEM_TYPES && !visited_types[tag]) {
		/* If the search critieron was not visited during the
		   sweep through the song's tag, it means this field
		   is absent from the tag or empty. Thus, if the
		   searched string is also empty
		   then it's a match as well and we should return
		   true. */
		if (value.empty())
			return true;

		if (tag == TAG_ALBUM_ARTIST && visited_types[TAG_ARTIST]) {
			/* if we're looking for "album artist", but
			   only "artist" exists, use that */
			for (const auto &item : _tag)
				if (item.type == TAG_ARTIST &&
				    StringMatchNN(item.value))
					return true;
		}
	}

	return false;
}

bool
SongFilter::Item::MatchNN(const LightSong &song) const noexcept
{
	if (tag == LOCATE_TAG_BASE_TYPE) {
		const auto uri = song.GetURI();
		return uri_is_child_or_same(value.c_str(), uri.c_str());
	}

	if (tag == LOCATE_TAG_MODIFIED_SINCE)
		return song.mtime >= time;

	if (tag == LOCATE_TAG_FILE_TYPE) {
		const auto uri = song.GetURI();
		return StringMatchNN(uri.c_str());
	}

	return MatchNN(song.tag);
}

SongFilter::SongFilter(unsigned tag, const char *value, bool fold_case)
{
	items.emplace_back(tag, value, fold_case);
}

SongFilter::~SongFilter()
{
	/* this destructor exists here just so it won't get inlined */
}

std::string
SongFilter::ToExpression() const noexcept
{
	auto i = items.begin();
	const auto end = items.end();

	if (std::next(i) == end)
		return i->ToExpression();

	std::string e("(");
	e += i->ToExpression();

	for (++i; i != end; ++i) {
		e += " AND ";
		e += i->ToExpression();
	}

	e.push_back(')');
	return e;
}

static std::chrono::system_clock::time_point
ParseTimeStamp(const char *s)
{
	assert(s != nullptr);

	char *endptr;
	unsigned long long value = strtoull(s, &endptr, 10);
	if (*endptr == 0 && endptr > s)
		/* it's an integral UNIX time stamp */
		return std::chrono::system_clock::from_time_t((time_t)value);

	/* try ISO 8601 */
	return ParseISO8601(s);
}

static constexpr bool
IsTagNameChar(char ch) noexcept
{
	return IsAlphaASCII(ch) || ch == '_' || ch == '-';
}

static const char *
FirstNonTagNameChar(const char *s) noexcept
{
	while (IsTagNameChar(*s))
		++s;
	return s;
}

static auto
ExpectFilterType(const char *&s)
{
	const char *end = FirstNonTagNameChar(s);
	if (end == s)
		throw std::runtime_error("Tag name expected");

	const std::string name(s, end);
	s = StripLeft(end);

	const auto type = locate_parse_type(name.c_str());
	if (type == TAG_NUM_OF_ITEM_TYPES)
		throw FormatRuntimeError("Unknown filter type: %s",
					 name.c_str());

	return type;
}

static constexpr bool
IsQuote(char ch) noexcept
{
	return ch == '"' || ch == '\'';
}

static std::string
ExpectQuoted(const char *&s)
{
	const char quote = *s++;
	if (!IsQuote(quote))
		throw std::runtime_error("Quoted string expected");

	const char *begin = s;
	const char *end = strchr(s, quote);
	if (end == nullptr)
		throw std::runtime_error("Closing quote not found");

	s = StripLeft(end + 1);
	return {begin, end};
}

const char *
SongFilter::ParseExpression(const char *s, bool fold_case)
{
	assert(*s == '(');

	s = StripLeft(s + 1);

	if (*s == '(')
		throw std::runtime_error("Nested expressions not yet implemented");

	const auto type = ExpectFilterType(s);

	if (type == LOCATE_TAG_MODIFIED_SINCE) {
		const auto value_s = ExpectQuoted(s);
		if (*s != ')')
			throw std::runtime_error("')' expected");
		items.emplace_back(type, ParseTimeStamp(value_s.c_str()));
		return StripLeft(s + 1);
	} else if (type == LOCATE_TAG_BASE_TYPE) {
		auto value = ExpectQuoted(s);
		if (*s != ')')
			throw std::runtime_error("')' expected");

		items.emplace_back(type, std::move(value), fold_case);
		return StripLeft(s + 1);
	} else {
		bool negated = false;
		if (s[0] == '!' && s[1] == '=')
			negated = true;
		else if (s[0] != '=' || s[1] != '=')
			throw std::runtime_error("'==' or '!=' expected");

		s = StripLeft(s + 2);
		auto value = ExpectQuoted(s);
		if (*s != ')')
			throw std::runtime_error("')' expected");

		items.emplace_back(type, std::move(value), fold_case);
		items.back().SetNegated(negated);
		return StripLeft(s + 1);
	}
}

void
SongFilter::Parse(const char *tag_string, const char *value, bool fold_case)
{
	unsigned tag = locate_parse_type(tag_string);
	if (tag == TAG_NUM_OF_ITEM_TYPES)
		throw std::runtime_error("Unknown filter type");

	if (tag == LOCATE_TAG_BASE_TYPE) {
		if (!uri_safe_local(value))
			throw std::runtime_error("Bad URI");

		/* case folding doesn't work with "base" */
		fold_case = false;
	}

	if (tag == LOCATE_TAG_MODIFIED_SINCE)
		items.emplace_back(tag, ParseTimeStamp(value));
	else
		items.emplace_back(tag, value, fold_case);
}

void
SongFilter::Parse(ConstBuffer<const char *> args, bool fold_case)
{
	if (args.empty())
		throw std::runtime_error("Incorrect number of filter arguments");

	do {
		if (*args.front() == '(') {
			const char *s = args.shift();
			const char *end = ParseExpression(s, fold_case);
			if (*end != 0)
				throw std::runtime_error("Unparsed garbage after expression");

			continue;
		}

		if (args.size < 2)
			throw std::runtime_error("Incorrect number of filter arguments");

		const char *tag = args.shift();
		const char *value = args.shift();
		Parse(tag, value, fold_case);
	} while (!args.empty());
}

bool
SongFilter::Match(const LightSong &song) const noexcept
{
	for (const auto &i : items)
		if (!i.Match(song))
			return false;

	return true;
}

bool
SongFilter::HasOtherThanBase() const noexcept
{
	for (const auto &i : items)
		if (i.GetTag() != LOCATE_TAG_BASE_TYPE)
			return true;

	return false;
}

const char *
SongFilter::GetBase() const noexcept
{
	for (const auto &i : items)
		if (i.GetTag() == LOCATE_TAG_BASE_TYPE)
			return i.GetValue();

	return nullptr;
}

SongFilter
SongFilter::WithoutBasePrefix(const char *_prefix) const noexcept
{
	const StringView prefix(_prefix);
	SongFilter result;

	for (const auto &i : items) {
		if (i.GetTag() == LOCATE_TAG_BASE_TYPE) {
			const char *s = StringAfterPrefix(i.GetValue(), prefix);
			if (s != nullptr) {
				if (*s == 0)
					continue;

				if (*s == '/') {
					++s;

					if (*s != 0)
						result.items.emplace_back(LOCATE_TAG_BASE_TYPE, s);

					continue;
				}
			}
		}

		result.items.emplace_back(i);
	}

	return result;
}
