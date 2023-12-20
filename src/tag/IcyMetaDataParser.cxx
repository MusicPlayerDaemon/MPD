// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "IcyMetaDataParser.hxx"
#include "tag/Builder.hxx"
#include "util/AllocatedString.hxx"
#include "util/StringSplit.hxx"

#include <algorithm>
#include <cassert>
#include <string_view>

#include <string.h>

using std::string_view_literals::operator""sv;

#ifdef HAVE_ICU_CONVERTER

void
IcyMetaDataParser::SetCharset(const char *charset)
{
	icu_converter = IcuConverter::Create(charset);
}

#endif

void
IcyMetaDataParser::Reset() noexcept
{
	if (!IsDefined())
		return;

	if (data_rest == 0 && meta_size > 0)
		delete[] meta_data;

	tag.reset();

	data_rest = data_size;
	meta_size = 0;
}

size_t
IcyMetaDataParser::Data(size_t length) noexcept
{
	assert(length > 0);

	if (!IsDefined())
		return length;

	if (data_rest == 0)
		return 0;

	if (length >= data_rest) {
		length = data_rest;
		data_rest = 0;
	} else
		data_rest -= length;

	return length;
}

static void
icy_add_item(TagBuilder &tag, TagType type, std::string_view value) noexcept
{
	if (value.size() >= 2 && value.front() == '\'' && value.back() == '\'') {
		/* strip the single quotes */
		value = value.substr(1, value.size() - 2);
	}

	if (!value.empty())
		tag.AddItem(type, value);
}

static void
icy_parse_tag_item(TagBuilder &tag,
#ifdef HAVE_ICU_CONVERTER
		   const IcuConverter *icu_converter,
#endif
		   std::string_view name, std::string_view value) noexcept
{
	if (name == "StreamTitle"sv) {
#ifdef HAVE_ICU_CONVERTER
		if (icu_converter != nullptr) {
			try {
				icy_add_item(tag, TAG_TITLE,
					     icu_converter->ToUTF8(value).c_str());
			} catch (...) {
			}

			return;
		}
#endif

		icy_add_item(tag, TAG_TITLE, value);
	}
}

/**
 * Find a single quote that is followed by a semicolon (or by the end
 * of the string).  If that fails, return the first single quote.  If
 * that also fails, return #end.
 */
static constexpr std::pair<std::string_view, std::string_view>
SplitEndQuote(std::string_view s) noexcept
{
	auto quote = s.find('\'');
	if (quote == s.npos)
		return {};

	if (const auto i = s.find("';"sv, quote); i != s.npos)
		quote = i;
	else
		quote = s.rfind('\'');

	assert(quote != s.npos);

	return {s.substr(0, quote), s.substr(quote + 1)};
}

static std::unique_ptr<Tag>
icy_parse_tag(
#ifdef HAVE_ICU_CONVERTER
	      const IcuConverter *icu_converter,
#endif
	      std::string_view src) noexcept
{
	TagBuilder tag;

	while (!src.empty()) {
		const auto [name, rest] = Split(src, '=');
		if (rest.empty())
			break;

		if (rest.front() != '\'') {
			/* syntax error; skip to the next semicolon,
			   try to recover */
			src = Split(rest, ';').second;
			continue;
		}

		src = rest.substr(1);

		const auto [value, after_value] = SplitEndQuote(rest.substr(1));
		if (after_value.data() == nullptr)
			break;

		icy_parse_tag_item(tag,
#ifdef HAVE_ICU_CONVERTER
				   icu_converter,
#endif
				   name, value);

		src = Split(after_value, ';').second;
	}

	return tag.CommitNew();
}

std::size_t
IcyMetaDataParser::Meta(std::span<const std::byte> src) noexcept
{
	assert(IsDefined());
	assert(data_rest == 0);
	assert(!src.empty());

	std::size_t consumed = 0;

	if (meta_size == 0) {
		/* read meta_size from the first byte of a meta
		   block */
		meta_size = static_cast<std::size_t>(src.front()) * 16;
		if (meta_size == 0) {
			/* special case: no metadata */
			data_rest = data_size;
			return 1;
		}

		src = src.subspan(1);

		/* 1 byte was consumed */
		++consumed;

		/* initialize metadata reader, allocate enough
		   memory (+1 for the null terminator) */
		meta_position = 0;
		meta_data = new char[meta_size + 1];
	}

	assert(meta_position < meta_size);

	if (src.size() > meta_size - meta_position)
		src = src.first(meta_size - meta_position);

	memcpy(meta_data + meta_position, src.data(), src.size());
	meta_position += src.size();
	consumed += src.size();

	if (meta_position == meta_size) {
		/* parse */

		tag = icy_parse_tag(
#ifdef HAVE_ICU_CONVERTER
				    icu_converter.get(),
#endif
				    {meta_data, meta_size});
		delete[] meta_data;

		/* change back to normal data mode */

		meta_size = 0;
		data_rest = data_size;
	}

	return consumed;
}

size_t
IcyMetaDataParser::ParseInPlace(std::span<std::byte> buffer) noexcept
{
	const auto begin = buffer.begin();
	auto dest = begin;
	auto src = buffer;

	while (!src.empty()) {
		std::size_t chunk = Data(src.size());
		if (chunk > 0) {
			dest = std::copy_n(src.begin(), chunk, dest);
			src = src.subspan(chunk);
			if (src.empty())
				break;
		}

		chunk = Meta(src);
		if (chunk > 0)
			src = src.subspan(chunk);
	}

	return std::distance(begin, dest);
}
