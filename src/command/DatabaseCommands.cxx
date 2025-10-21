// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "DatabaseCommands.hxx"
#include "PositionArg.hxx"
#include "Request.hxx"
#include "Partition.hxx"
#include "client/StringNormalization.hxx"
#include "db/DatabaseQueue.hxx"
#include "db/DatabasePlaylist.hxx"
#include "db/DatabasePrint.hxx"
#include "db/Count.hxx"
#include "db/Selection.hxx"
#include "protocol/RangeArg.hxx"
#include "client/Client.hxx"
#include "client/Response.hxx"
#include "tag/Names.hxx"
#include "tag/ParseName.hxx"
#include "util/Exception.hxx"
#include "util/StringAPI.hxx"
#include "util/ASCII.hxx"
#include "song/Filter.hxx"

#include <fmt/format.h>

#include <memory>
#include <vector>

#include <limits.h> // for UINT_MAX

CommandResult
handle_listfiles_db(Client &client, Response &r, const char *uri)
{
	const DatabaseSelection selection(uri, false);
	db_selection_print(r, client.GetPartition(),
			   selection, false, true);
	return CommandResult::OK;
}

CommandResult
handle_lsinfo2(Client &client, const char *uri, Response &r)
{
	const DatabaseSelection selection(uri, false);
	db_selection_print(r, client.GetPartition(),
			   selection, true, false);
	return CommandResult::OK;
}

static TagType
ParseSortTag(const char *s)
{
	if (StringIsEqualIgnoreCase(s, "Last-Modified"))
		return TagType(SORT_TAG_LAST_MODIFIED);

	if (StringIsEqualIgnoreCase(s, "Added"))
		return TagType(SORT_TAG_ADDED);

	TagType tag = tag_name_parse_i(s);
	if (tag == TAG_NUM_OF_ITEM_TYPES)
		throw ProtocolError(ACK_ERROR_ARG, "Unknown sort tag");

	return tag;
}

static unsigned
ParseQueuePosition(Request &args, unsigned queue_length)
{
	if (args.size() >= 2 && StringIsEqual(args[args.size() - 2], "position")) {
		unsigned position = args.ParseUnsigned(args.size() - 1,
						       queue_length);
		args.pop_back();
		args.pop_back();
		return position;
	}

	/* append to the end of the queue by default */
	return queue_length;
}

static unsigned
ParseInsertPosition(Request &args, const playlist &playlist)
{
	if (args.size() >= 2 && StringIsEqual(args[args.size() - 2], "position")) {
		unsigned position = ParseInsertPosition(args.back(), playlist);
		args.pop_back();
		args.pop_back();
		return position;
	}

	/* append to the end of the queue by default */
	return playlist.queue.GetLength();
}

/**
 * Convert all remaining arguments to a #DatabaseSelection.
 *
 * @param filter a buffer to be used for DatabaseSelection::filter
 */
static DatabaseSelection
ParseDatabaseSelection(Request args, bool fold_case, bool strip_diacritics, SongFilter &filter)
{
	RangeArg window = RangeArg::All();
	if (args.size() >= 2 && StringIsEqual(args[args.size() - 2], "window")) {
		window = args.ParseRange(args.size() - 1);

		args.pop_back();
		args.pop_back();
	}

	TagType sort = TAG_NUM_OF_ITEM_TYPES;
	bool descending = false;
	if (args.size() >= 2 && StringIsEqual(args[args.size() - 2], "sort")) {
		const char *s = args.back();
		if (*s == '-') {
			descending = true;
			++s;
		}

		sort = ParseSortTag(s);

		args.pop_back();
		args.pop_back();
	}

	try {
		filter.Parse(args, fold_case, strip_diacritics);
	} catch (...) {
		throw ProtocolError(ACK_ERROR_ARG,
				    GetFullMessage(std::current_exception()).c_str());
	}
	filter.Optimize();

	DatabaseSelection selection("", true, &filter);
	selection.window = window;
	selection.sort = sort;
	selection.descending = descending;
	return selection;
}

static CommandResult
handle_match(Client &client, Request args, Response &r, bool fold_case, bool strip_diacritics)
{
	SongFilter filter;
	const auto selection = ParseDatabaseSelection(args, fold_case, strip_diacritics, filter);

	db_selection_print(r, client.GetPartition(),
			   selection, true, false);
	return CommandResult::OK;
}

CommandResult
handle_find(Client &client, Request args, Response &r)
{
	return handle_match(client, args, r, false, false);
}

CommandResult
handle_search(Client &client, Request args, Response &r)
{
	auto strip_diacritics = client.StringNormalizationEnabled(SN_STRIP_DIACRITICS);
	return handle_match(client, args, r, true, strip_diacritics);
}

static CommandResult
handle_match_add(Client &client, Request args, bool fold_case, bool strip_diacritics)
{
	auto &partition = client.GetPartition();
	const auto queue_length = partition.playlist.queue.GetLength();
	const unsigned position =
		ParseInsertPosition(args, partition.playlist);

	SongFilter filter;
	const auto selection = ParseDatabaseSelection(args, fold_case, strip_diacritics, filter);

	AddFromDatabase(partition, selection);

	if (position < queue_length) {
		const auto new_queue_length =
			partition.playlist.queue.GetLength();
		const RangeArg range{queue_length, new_queue_length};

		try {
			partition.MoveRange(range, position);
		} catch (...) {
			/* ignore - shall we handle it? */
		}
	}

	return CommandResult::OK;
}

static CommandResult
handle_match_addpl(Client &client, Request args, bool fold_case) {
	const char *playlist = args.shift();

	const unsigned position = ParseQueuePosition(args, UINT_MAX);

	SongFilter filter;
	auto strip_diacritics = client.StringNormalizationEnabled(SN_STRIP_DIACRITICS);
	const auto selection = ParseDatabaseSelection(args, fold_case, strip_diacritics, filter);

	const Database &db = client.GetDatabaseOrThrow();

	if (position == UINT_MAX)
		search_add_to_playlist(db, client.GetStorage(),
				       playlist, selection);
	else
		SearchInsertIntoPlaylist(db, client.GetStorage(), selection,
					 playlist, position);

	return CommandResult::OK;
}

CommandResult
handle_findadd(Client &client, Request args, Response &)
{
	return handle_match_add(client, args, false, false);
}

CommandResult
handle_searchadd(Client &client, Request args, Response &)
{
	auto strip_diacritics = client.StringNormalizationEnabled(SN_STRIP_DIACRITICS);
	return handle_match_add(client, args, true,  strip_diacritics);
}

CommandResult
handle_findaddpl(Client &client, Request args, Response &)
{
	return handle_match_addpl(client, args, false);
}

CommandResult
handle_searchaddpl(Client &client, Request args, Response &)
{
	return handle_match_addpl(client, args, true);
}

static CommandResult
handle_count_internal(Client &client, Request args, Response &r, bool fold_case, bool strip_diacritics)
{
	TagType group = TAG_NUM_OF_ITEM_TYPES;
	if (args.size() >= 2 && StringIsEqual(args[args.size() - 2], "group")) {
		const char *s = args[args.size() - 1];
		group = tag_name_parse_i(s);
		if (group == TAG_NUM_OF_ITEM_TYPES) {
			r.FmtError(ACK_ERROR_ARG,
				   "Unknown tag type: {}", s);
			return CommandResult::ERROR;
		}

		args.pop_back();
		args.pop_back();
	}

	SongFilter filter;
	if (!args.empty()) {
		try {
			filter.Parse(args, fold_case, strip_diacritics);
		} catch (...) {
			r.Error(ACK_ERROR_ARG,
				GetFullMessage(std::current_exception()).c_str());
			return CommandResult::ERROR;
		}

		filter.Optimize();
	}

	PrintSongCount(r, client.GetPartition(), "", &filter, group);
	return CommandResult::OK;
}

CommandResult
handle_count(Client &client, Request args, Response &r)
{
	return handle_count_internal(client, args, r, false, false);
}

CommandResult
handle_searchcount(Client &client, Request args, Response &r)
{
	auto strip_diacritics = client.StringNormalizationEnabled(SN_STRIP_DIACRITICS);
	return handle_count_internal(client, args, r, true, strip_diacritics);
}

CommandResult
handle_listall(Client &client, Request args, Response &r)
{
	/* default is root directory */
	const auto uri = args.GetOptional(0, "");

	db_selection_print(r, client.GetPartition(),
			   DatabaseSelection(uri, true),
			   false, false);
	return CommandResult::OK;
}

static CommandResult
handle_list_file(Client &client, Request args, Response &r)
{
	std::unique_ptr<SongFilter> filter;

	if (!args.empty()) {
		filter = std::make_unique<SongFilter>();
		try {
			filter->Parse(args, false);
		} catch (...) {
			r.Error(ACK_ERROR_ARG,
				GetFullMessage(std::current_exception()).c_str());
			return CommandResult::ERROR;
		}
		filter->Optimize();
	}

	PrintSongUris(r, client.GetPartition(), filter.get());
	return CommandResult::OK;
}

CommandResult
handle_list(Client &client, Request args, Response &r)
{
	const char *tag_name = args.shift();
	if (StringEqualsCaseASCII(tag_name, "file") ||
	    StringEqualsCaseASCII(tag_name, "filename"))
		return handle_list_file(client, args, r);

	const auto tagType = tag_name_parse_i(tag_name);
	if (tagType == TAG_NUM_OF_ITEM_TYPES) {
		r.FmtError(ACK_ERROR_ARG,
			   "Unknown tag type: {}", tag_name);
		return CommandResult::ERROR;
	}

	RangeArg window = RangeArg::All();
	if (args.size() >= 2 && StringIsEqual(args[args.size() - 2], "window")) {
		window = args.ParseRange(args.size() - 1);

		args.pop_back();
		args.pop_back();
	}

	std::unique_ptr<SongFilter> filter;
	std::vector<TagType> tag_types;

	if (args.size() == 1 &&
	    /* parantheses are the syntax for filter expressions: no
	       compatibility mode */
	    args.front()[0] != '(') {
		/* for compatibility with < 0.12.0 */
		if (tagType != TAG_ALBUM) {
			r.FmtError(ACK_ERROR_ARG,
				   "should be {:?} for 3 arguments",
				   tag_item_names[TAG_ALBUM]);
			return CommandResult::ERROR;
		}

		filter = std::make_unique<SongFilter>(TAG_ARTIST,
					    args.shift());
	}

	while (args.size() >= 2 &&
	       StringIsEqual(args[args.size() - 2], "group")) {
		const char *s = args[args.size() - 1];
		const auto group = tag_name_parse_i(s);
		if (group == TAG_NUM_OF_ITEM_TYPES) {
			r.FmtError(ACK_ERROR_ARG,
				   "Unknown tag type: {}", s);
			return CommandResult::ERROR;
		}

		if (group == tagType ||
		    std::find(tag_types.begin(), tag_types.end(),
			      group) != tag_types.end()) {
			r.Error(ACK_ERROR_ARG, "Conflicting group");
			return CommandResult::ERROR;
		}

		tag_types.emplace_back(group);

		args.pop_back();
		args.pop_back();
	}

	tag_types.emplace_back(tagType);

	if (!args.empty()) {
		filter = std::make_unique<SongFilter>();
		try {
			filter->Parse(args, false);
		} catch (...) {
			r.Error(ACK_ERROR_ARG,
				GetFullMessage(std::current_exception()).c_str());
			return CommandResult::ERROR;
		}
		filter->Optimize();
	}

	PrintUniqueTags(r, client.GetPartition(),
			{&tag_types.front(), tag_types.size()},
			filter.get(),
			window);
	return CommandResult::OK;
}

CommandResult
handle_listallinfo(Client &client, Request args, Response &r)
{
	/* default is root directory */
	const auto uri = args.GetOptional(0, "");

	db_selection_print(r, client.GetPartition(),
			   DatabaseSelection(uri, true),
			   true, false);
	return CommandResult::OK;
}
