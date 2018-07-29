/*
 * Copyright 2003-2018 The Music Player Daemon Project
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

/**
 * Limit the search to files within the given directory.
 */
#define LOCATE_TAG_BASE_TYPE (TAG_NUM_OF_ITEM_TYPES + 1)
#define LOCATE_TAG_MODIFIED_SINCE (TAG_NUM_OF_ITEM_TYPES + 2)
#define LOCATE_TAG_FILE_TYPE	TAG_NUM_OF_ITEM_TYPES+10
#define LOCATE_TAG_ANY_TYPE     TAG_NUM_OF_ITEM_TYPES+20

/**
 * @return #TAG_NUM_OF_ITEM_TYPES on error
 */
gcc_pure
static unsigned
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

bool
StringFilter::Match(const char *s) const noexcept
{
#if !CLANG_CHECK_VERSION(3,6)
	/* disabled on clang due to -Wtautological-pointer-compare */
	assert(s != nullptr);
#endif

	if (fold_case) {
		return fold_case.IsIn(s);
	} else {
		return StringIsEqual(s, value.c_str());
	}
}

std::string
UriSongFilter::ToExpression() const noexcept
{
	return std::string("(" LOCATE_TAG_FILE_KEY " ") + (negated ? "!=" : "==") + " \"" + filter.GetValue() + "\")";
}

bool
UriSongFilter::Match(const LightSong &song) const noexcept
{
	return filter.Match(song.GetURI().c_str()) != negated;
}

std::string
BaseSongFilter::ToExpression() const noexcept
{
	return "(base \"" + value + "\")";
}

bool
BaseSongFilter::Match(const LightSong &song) const noexcept
{
	return uri_is_child_or_same(value.c_str(), song.GetURI().c_str());
}

std::string
TagSongFilter::ToExpression() const noexcept
{
	const char *name = type == TAG_NUM_OF_ITEM_TYPES
		? LOCATE_TAG_ANY_KEY
		: tag_item_names[type];

	return std::string("(") + name + " " + (negated ? "!=" : "==") + " \"" + filter.GetValue() + "\")";
}

bool
TagSongFilter::MatchNN(const TagItem &item) const noexcept
{
	return (type == TAG_NUM_OF_ITEM_TYPES || item.type == type) &&
		filter.Match(item.value);
}

bool
TagSongFilter::MatchNN(const Tag &tag) const noexcept
{
	bool visited_types[TAG_NUM_OF_ITEM_TYPES];
	std::fill_n(visited_types, size_t(TAG_NUM_OF_ITEM_TYPES), false);

	for (const auto &i : tag) {
		visited_types[i.type] = true;

		if (MatchNN(i))
			return true;
	}

	if (type < TAG_NUM_OF_ITEM_TYPES && !visited_types[type]) {
		/* If the search critieron was not visited during the
		   sweep through the song's tag, it means this field
		   is absent from the tag or empty. Thus, if the
		   searched string is also empty
		   then it's a match as well and we should return
		   true. */
		if (filter.empty())
			return true;

		if (type == TAG_ALBUM_ARTIST && visited_types[TAG_ARTIST]) {
			/* if we're looking for "album artist", but
			   only "artist" exists, use that */
			for (const auto &item : tag)
				if (item.type == TAG_ARTIST &&
				    filter.Match(item.value))
					return true;
		}
	}

	return false;
}

bool
TagSongFilter::Match(const LightSong &song) const noexcept
{
	return MatchNN(song.tag) != negated;
}

std::string
ModifiedSinceSongFilter::ToExpression() const noexcept
{
	return std::string("(modified-since \"") + FormatISO8601(value).c_str() + "\")";
}

bool
ModifiedSinceSongFilter::Match(const LightSong &song) const noexcept
{
	return song.mtime >= value;
}

ISongFilterPtr
AndSongFilter::Clone() const noexcept
{
	auto result = std::make_unique<AndSongFilter>();

	for (const auto &i : items)
		result->items.emplace_back(i->Clone());

	return result;
}

std::string
AndSongFilter::ToExpression() const noexcept
{
	auto i = items.begin();
	const auto end = items.end();

	if (std::next(i) == end)
		return (*i)->ToExpression();

	std::string e("(");
	e += (*i)->ToExpression();

	for (++i; i != end; ++i) {
		e += " AND ";
		e += (*i)->ToExpression();
	}

	e.push_back(')');
	return e;
}

bool
AndSongFilter::Match(const LightSong &song) const noexcept
{
	for (const auto &i : items)
		if (!i->Match(song))
			return false;

	return true;
}

SongFilter::SongFilter(TagType tag, const char *value, bool fold_case)
{
	and_filter.AddItem(std::make_unique<TagSongFilter>(tag, value,
							   fold_case, false));
}

SongFilter::~SongFilter()
{
	/* this destructor exists here just so it won't get inlined */
}

std::string
SongFilter::ToExpression() const noexcept
{
	return and_filter.ToExpression();
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

ISongFilterPtr
SongFilter::ParseExpression(const char *&s, bool fold_case)
{
	assert(*s == '(');

	s = StripLeft(s + 1);

	if (*s == '(')
		throw std::runtime_error("Nested expressions not yet implemented");

	auto type = ExpectFilterType(s);

	if (type == LOCATE_TAG_MODIFIED_SINCE) {
		const auto value_s = ExpectQuoted(s);
		if (*s != ')')
			throw std::runtime_error("')' expected");
		s = StripLeft(s + 1);
		return std::make_unique<ModifiedSinceSongFilter>(ParseTimeStamp(value_s.c_str()));
	} else if (type == LOCATE_TAG_BASE_TYPE) {
		auto value = ExpectQuoted(s);
		if (*s != ')')
			throw std::runtime_error("')' expected");
		s = StripLeft(s + 1);

		return std::make_unique<BaseSongFilter>(std::move(value));
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

		s = StripLeft(s + 1);

		if (type == LOCATE_TAG_ANY_TYPE)
			type = TAG_NUM_OF_ITEM_TYPES;

		if (type == LOCATE_TAG_FILE_TYPE)
			return std::make_unique<UriSongFilter>(std::move(value),
							       fold_case,
							       negated);

		return std::make_unique<TagSongFilter>(TagType(type),
						       std::move(value),
						       fold_case, negated);
	}
}

void
SongFilter::Parse(const char *tag_string, const char *value, bool fold_case)
{
	unsigned tag = locate_parse_type(tag_string);

	switch (tag) {
	case TAG_NUM_OF_ITEM_TYPES:
		throw std::runtime_error("Unknown filter type");

	case LOCATE_TAG_BASE_TYPE:
		if (!uri_safe_local(value))
			throw std::runtime_error("Bad URI");

		and_filter.AddItem(std::make_unique<BaseSongFilter>(value));
		break;

	case LOCATE_TAG_MODIFIED_SINCE:
		and_filter.AddItem(std::make_unique<ModifiedSinceSongFilter>(ParseTimeStamp(value)));
		break;

	case LOCATE_TAG_FILE_TYPE:
		and_filter.AddItem(std::make_unique<UriSongFilter>(value,
								   fold_case,
								   false));
		break;

	default:
		if (tag == LOCATE_TAG_ANY_TYPE)
			tag = TAG_NUM_OF_ITEM_TYPES;

		and_filter.AddItem(std::make_unique<TagSongFilter>(TagType(tag),
								   value,
								   fold_case,
								   false));
		break;
	}
}

void
SongFilter::Parse(ConstBuffer<const char *> args, bool fold_case)
{
	if (args.empty())
		throw std::runtime_error("Incorrect number of filter arguments");

	do {
		if (*args.front() == '(') {
			const char *s = args.shift();
			const char *end = s;
			auto f = ParseExpression(end, fold_case);
			if (*end != 0)
				throw std::runtime_error("Unparsed garbage after expression");

			and_filter.AddItem(std::move(f));
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
	return and_filter.Match(song);
}

bool
SongFilter::HasFoldCase() const noexcept
{
	for (const auto &i : and_filter.GetItems()) {
		if (auto t = dynamic_cast<const TagSongFilter *>(i.get())) {
			if (t->GetFoldCase())
				return true;
		} else if (auto u = dynamic_cast<const UriSongFilter *>(i.get())) {
			if (u->GetFoldCase())
				return true;
		}
	}

	return false;
}

bool
SongFilter::HasOtherThanBase() const noexcept
{
	for (const auto &i : and_filter.GetItems()) {
		const auto *f = dynamic_cast<const BaseSongFilter *>(i.get());
		if (f == nullptr)
			return true;
	}

	return false;
}

const char *
SongFilter::GetBase() const noexcept
{
	for (const auto &i : and_filter.GetItems()) {
		const auto *f = dynamic_cast<const BaseSongFilter *>(i.get());
		if (f != nullptr)
			return f->GetValue();
	}

	return nullptr;
}

SongFilter
SongFilter::WithoutBasePrefix(const char *_prefix) const noexcept
{
	const StringView prefix(_prefix);
	SongFilter result;

	for (const auto &i : and_filter.GetItems()) {
		const auto *f = dynamic_cast<const BaseSongFilter *>(i.get());
		if (f != nullptr) {
			const char *s = StringAfterPrefix(f->GetValue(), prefix);
			if (s != nullptr) {
				if (*s == 0)
					continue;

				if (*s == '/') {
					++s;

					if (*s != 0)
						result.and_filter.AddItem(std::make_unique<BaseSongFilter>(s));

					continue;
				}
			}
		}

		result.and_filter.AddItem(i->Clone());
	}

	return result;
}
