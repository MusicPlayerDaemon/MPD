/*
 * Copyright (C) 2003-2012 The Music Player Daemon Project
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
#include "CommandError.h"
#include "client_internal.h"
#include "tag.h"
#include "uri.h"

extern "C" {
#include "db_print.h"
#include "db_selection.h"
#include "dbUtils.h"
#include "locate.h"
#include "protocol/result.h"
}

#include <assert.h>
#include <string.h>

enum command_return
handle_lsinfo2(struct client *client, int argc, char *argv[])
{
	const char *uri;

	if (argc == 2)
		uri = argv[1];
	else
		/* default is root directory */
		uri = "";

	struct db_selection selection;
	db_selection_init(&selection, uri, false);

	GError *error = NULL;
	if (!db_selection_print(client, &selection, true, &error))
		return print_error(client, error);

	return COMMAND_RETURN_OK;
}

enum command_return
handle_find(struct client *client, int argc, char *argv[])
{
	struct locate_item_list *list =
		locate_item_list_parse(argv + 1, argc - 1);

	if (list == NULL || list->length == 0) {
		if (list != NULL)
			locate_item_list_free(list);

		command_error(client, ACK_ERROR_ARG, "incorrect arguments");
		return COMMAND_RETURN_ERROR;
	}

	GError *error = NULL;
	enum command_return ret = findSongsIn(client, "", list, &error)
		? COMMAND_RETURN_OK
		: print_error(client, error);

	locate_item_list_free(list);

	return ret;
}

enum command_return
handle_findadd(struct client *client, int argc, char *argv[])
{
    struct locate_item_list *list =
	    locate_item_list_parse(argv + 1, argc - 1);
    if (list == NULL || list->length == 0) {
	    if (list != NULL)
		    locate_item_list_free(list);

	    command_error(client, ACK_ERROR_ARG, "incorrect arguments");
	    return COMMAND_RETURN_ERROR;
    }

    GError *error = NULL;
    enum command_return ret =
	    findAddIn(client->player_control, "", list, &error)
	    ? COMMAND_RETURN_OK
	    : print_error(client, error);

    locate_item_list_free(list);

    return ret;
}

enum command_return
handle_search(struct client *client, int argc, char *argv[])
{
	struct locate_item_list *list =
		locate_item_list_parse(argv + 1, argc - 1);

	if (list == NULL || list->length == 0) {
		if (list != NULL)
			locate_item_list_free(list);

		command_error(client, ACK_ERROR_ARG, "incorrect arguments");
		return COMMAND_RETURN_ERROR;
	}

	GError *error = NULL;
	enum command_return ret = searchForSongsIn(client, "", list, &error)
		? COMMAND_RETURN_OK
		: print_error(client, error);

	locate_item_list_free(list);

	return ret;
}

enum command_return
handle_searchadd(struct client *client, int argc, char *argv[])
{
	struct locate_item_list *list =
		locate_item_list_parse(argv + 1, argc - 1);

	if (list == NULL || list->length == 0) {
		if (list != NULL)
			locate_item_list_free(list);

		command_error(client, ACK_ERROR_ARG, "incorrect arguments");
		return COMMAND_RETURN_ERROR;
	}

	GError *error = NULL;
	enum command_return ret = search_add_songs(client->player_control,
						   "", list, &error)
		? COMMAND_RETURN_OK
		: print_error(client, error);

	locate_item_list_free(list);

	return ret;
}

enum command_return
handle_searchaddpl(struct client *client, int argc, char *argv[])
{
	const char *playlist = argv[1];

	struct locate_item_list *list =
		locate_item_list_parse(argv + 2, argc - 2);

	if (list == NULL || list->length == 0) {
		if (list != NULL)
			locate_item_list_free(list);

		command_error(client, ACK_ERROR_ARG, "incorrect arguments");
		return COMMAND_RETURN_ERROR;
	}

	GError *error = NULL;
	enum command_return ret =
		search_add_to_playlist("", playlist, list, &error)
		? COMMAND_RETURN_OK
		: print_error(client, error);

	locate_item_list_free(list);

	return ret;
}

enum command_return
handle_count(struct client *client, int argc, char *argv[])
{
	struct locate_item_list *list =
		locate_item_list_parse(argv + 1, argc - 1);

	if (list == NULL || list->length == 0) {
		if (list != NULL)
			locate_item_list_free(list);

		command_error(client, ACK_ERROR_ARG, "incorrect arguments");
		return COMMAND_RETURN_ERROR;
	}

	GError *error = NULL;
	enum command_return ret =
		searchStatsForSongsIn(client, "", list, &error)
		? COMMAND_RETURN_OK
		: print_error(client, error);

	locate_item_list_free(list);

	return ret;
}

enum command_return
handle_listall(struct client *client, G_GNUC_UNUSED int argc, char *argv[])
{
	const char *directory = "";

	if (argc == 2)
		directory = argv[1];

	GError *error = NULL;
	return printAllIn(client, directory, &error)
		? COMMAND_RETURN_OK
		: print_error(client, error);
}

enum command_return
handle_list(struct client *client, int argc, char *argv[])
{
	struct locate_item_list *conditionals;
	int tagType = locate_parse_type(argv[1]);

	if (tagType < 0) {
		command_error(client, ACK_ERROR_ARG, "\"%s\" is not known", argv[1]);
		return COMMAND_RETURN_ERROR;
	}

	if (tagType == LOCATE_TAG_ANY_TYPE) {
		command_error(client, ACK_ERROR_ARG,
			      "\"any\" is not a valid return tag type");
		return COMMAND_RETURN_ERROR;
	}

	/* for compatibility with < 0.12.0 */
	if (argc == 3) {
		if (tagType != TAG_ALBUM) {
			command_error(client, ACK_ERROR_ARG,
				      "should be \"%s\" for 3 arguments",
				      tag_item_names[TAG_ALBUM]);
			return COMMAND_RETURN_ERROR;
		}

		locate_item_list_parse(argv + 1, argc - 1);

		conditionals = locate_item_list_new(1);
		conditionals->items[0].tag = TAG_ARTIST;
		conditionals->items[0].needle = g_strdup(argv[2]);
	} else {
		conditionals =
			locate_item_list_parse(argv + 2, argc - 2);
		if (conditionals == NULL) {
			command_error(client, ACK_ERROR_ARG,
				      "not able to parse args");
			return COMMAND_RETURN_ERROR;
		}
	}

	GError *error = NULL;
	enum command_return ret =
		listAllUniqueTags(client, tagType, conditionals, &error)
		? COMMAND_RETURN_OK
		: print_error(client, error);

	locate_item_list_free(conditionals);

	return ret;
}

enum command_return
handle_listallinfo(struct client *client, G_GNUC_UNUSED int argc, char *argv[])
{
	const char *directory = "";

	if (argc == 2)
		directory = argv[1];

	GError *error = NULL;
	return printInfoForAllIn(client, directory, &error)
		? COMMAND_RETURN_OK
		: print_error(client, error);
}
