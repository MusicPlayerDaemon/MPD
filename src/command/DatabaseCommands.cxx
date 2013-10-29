/*
 * Copyright (C) 2003-2013 The Music Player Daemon Project
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
#include "DatabaseCommands.hxx"
#include "DatabaseQueue.hxx"
#include "DatabasePlaylist.hxx"
#include "DatabasePrint.hxx"
#include "DatabaseSelection.hxx"
#include "CommandError.hxx"
#include "Client.hxx"
#include "tag/Tag.hxx"
#include "util/UriUtil.hxx"
#include "util/Error.hxx"
#include "SongFilter.hxx"
#include "protocol/Result.hxx"

#include <assert.h>
#include <string.h>

CommandResult
handle_lsinfo2(Client &client, int argc, char *argv[])
{
	const char *uri;

	if (argc == 2)
		uri = argv[1];
	else
		/* default is root directory */
		uri = "";

	const DatabaseSelection selection(uri, false);

	Error error;
	if (!db_selection_print(client, selection, true, error))
		return print_error(client, error);

	return CommandResult::OK;
}

static CommandResult
handle_match(Client &client, int argc, char *argv[], bool fold_case)
{
	SongFilter filter;
	if (!filter.Parse(argc - 1, argv + 1, fold_case)) {
		command_error(client, ACK_ERROR_ARG, "incorrect arguments");
		return CommandResult::ERROR;
	}

	const DatabaseSelection selection("", true, &filter);

	Error error;
	return db_selection_print(client, selection, true, error)
		? CommandResult::OK
		: print_error(client, error);
}

CommandResult
handle_find(Client &client, int argc, char *argv[])
{
	return handle_match(client, argc, argv, false);
}

CommandResult
handle_search(Client &client, int argc, char *argv[])
{
	return handle_match(client, argc, argv, true);
}

static CommandResult
handle_match_add(Client &client, int argc, char *argv[], bool fold_case)
{
	SongFilter filter;
	if (!filter.Parse(argc - 1, argv + 1, fold_case)) {
		command_error(client, ACK_ERROR_ARG, "incorrect arguments");
		return CommandResult::ERROR;
	}

	const DatabaseSelection selection("", true, &filter);
	Error error;
	return AddFromDatabase(client.partition, selection, error)
		? CommandResult::OK
		: print_error(client, error);
}

CommandResult
handle_findadd(Client &client, int argc, char *argv[])
{
	return handle_match_add(client, argc, argv, false);
}

CommandResult
handle_searchadd(Client &client, int argc, char *argv[])
{
	return handle_match_add(client, argc, argv, true);
}

CommandResult
handle_searchaddpl(Client &client, int argc, char *argv[])
{
	const char *playlist = argv[1];

	SongFilter filter;
	if (!filter.Parse(argc - 2, argv + 2, true)) {
		command_error(client, ACK_ERROR_ARG, "incorrect arguments");
		return CommandResult::ERROR;
	}

	Error error;
	return search_add_to_playlist("", playlist, &filter, error)
		? CommandResult::OK
		: print_error(client, error);
}

CommandResult
handle_count(Client &client, int argc, char *argv[])
{
	SongFilter filter;
	if (!filter.Parse(argc - 1, argv + 1, false)) {
		command_error(client, ACK_ERROR_ARG, "incorrect arguments");
		return CommandResult::ERROR;
	}

	Error error;
	return  searchStatsForSongsIn(client, "", &filter, error)
		? CommandResult::OK
		: print_error(client, error);
}

CommandResult
handle_listall(Client &client, gcc_unused int argc, char *argv[])
{
	const char *directory = "";

	if (argc == 2)
		directory = argv[1];

	Error error;
	return printAllIn(client, directory, error)
		? CommandResult::OK
		: print_error(client, error);
}

CommandResult
handle_list(Client &client, int argc, char *argv[])
{
	unsigned tagType = locate_parse_type(argv[1]);

	if (tagType == TAG_NUM_OF_ITEM_TYPES) {
		command_error(client, ACK_ERROR_ARG, "\"%s\" is not known", argv[1]);
		return CommandResult::ERROR;
	}

	if (tagType == LOCATE_TAG_ANY_TYPE) {
		command_error(client, ACK_ERROR_ARG,
			      "\"any\" is not a valid return tag type");
		return CommandResult::ERROR;
	}

	/* for compatibility with < 0.12.0 */
	SongFilter *filter;
	if (argc == 3) {
		if (tagType != TAG_ALBUM) {
			command_error(client, ACK_ERROR_ARG,
				      "should be \"%s\" for 3 arguments",
				      tag_item_names[TAG_ALBUM]);
			return CommandResult::ERROR;
		}

		filter = new SongFilter((unsigned)TAG_ARTIST, argv[2]);
	} else if (argc > 2) {
		filter = new SongFilter();
		if (!filter->Parse(argc - 2, argv + 2, false)) {
			delete filter;
			command_error(client, ACK_ERROR_ARG,
				      "not able to parse args");
			return CommandResult::ERROR;
		}
	} else
		filter = nullptr;

	Error error;
	CommandResult ret =
		listAllUniqueTags(client, tagType, filter, error)
		? CommandResult::OK
		: print_error(client, error);

	delete filter;

	return ret;
}

CommandResult
handle_listallinfo(Client &client, gcc_unused int argc, char *argv[])
{
	const char *directory = "";

	if (argc == 2)
		directory = argv[1];

	Error error;
	return printInfoForAllIn(client, directory, error)
		? CommandResult::OK
		: print_error(client, error);
}
