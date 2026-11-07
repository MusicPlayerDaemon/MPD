// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "ApeTag.hxx"
#include "ApeLoader.hxx"
#include "ParseName.hxx"
#include "Table.hxx"
#include "Handler.hxx"
#include "util/IterableSplitString.hxx"
#include "util/StringCompare.hxx"

using std::string_view_literals::operator""sv;

static constexpr struct tag_table ape_tags[] = {
	{ "album artist", TAG_ALBUM_ARTIST },
	{ "year", TAG_DATE },
	{ "musicbrainz_albumtype", TAG_RELEASETYPE },
	{ nullptr, TAG_NUM_OF_ITEM_TYPES }
};

[[gnu::pure]]
static unsigned
ParseApeLabelRank(std::string_view name) noexcept
{
	if (StringIsEqualIgnoreCase(name, "label"sv))
		return 1;
	if (StringIsEqualIgnoreCase(name, "publisher"sv))
		return 2;
	if (StringIsEqualIgnoreCase(name, "organization"sv))
		return 3;
	return 0;
}

static TagType
tag_ape_name_parse(const char *name)
{
	TagType type = tag_table_lookup_i(ape_tags, name);
	if (type == TAG_NUM_OF_ITEM_TYPES)
		type = tag_name_parse_i(name);

	return type;
}

/**
 * @return true if the item was recognized
 */
static bool
tag_ape_import_item(unsigned long flags,
		    const char *key, std::string_view value,
		    TagHandler &handler) noexcept
{
	/* we only care about utf-8 text tags */
	if ((flags & (0x3 << 1)) != 0)
		return false;

	if (handler.WantPair())
		for (const auto i : IterableSplitString(value, '\0'))
			handler.OnPair(key, i);

	TagType type = tag_ape_name_parse(key);
	if (type == TAG_NUM_OF_ITEM_TYPES || type == TAG_LABEL)
		return false;

	for (const auto i : IterableSplitString(value, '\0'))
		handler.OnTag(type, i);

	return true;
}

bool
tag_ape_scan2(InputStream &is, TagHandler &handler)
{
	bool recognized = false;
	unsigned label_rank = 4;
	std::string label_value;

	auto callback = [&](unsigned long flags, const char *key,
			    std::string_view value) {
		if ((flags & (0x3 << 1)) == 0) {
			if (const unsigned rank = ParseApeLabelRank(key); rank != 0) {
				if (handler.WantTag() && rank <= label_rank) {
					for (const auto i : IterableSplitString(value, '\0')) {
						if (!i.empty()) {
							if (rank < label_rank) {
								label_value.clear();
								label_rank = rank;
							} else if (!label_value.empty()) {
								label_value.push_back('\0');
							}
							label_value.append(i);
						}
					}
				}
				recognized = true;
			}
		}

		recognized |= tag_ape_import_item(flags, key, value, handler);
		return true;
	};

	if (!tag_ape_scan(is, callback))
		return false;

	for (const auto i : IterableSplitString(label_value, '\0'))
		handler.OnTag(TAG_LABEL, i);
	return recognized;
}
