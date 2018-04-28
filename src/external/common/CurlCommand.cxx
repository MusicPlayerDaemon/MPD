/*
 * Copyright 2015-2018 Cary Audio
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
#include "CurlCommand.hxx"
#include "util/Macros.hxx"

#include <sstream>
#include <cassert>

static const char *CommandTabel[] = { "unkown", "POST", "GET", "DELETE", "PUT" };


bool
CurlCommand::isDefined() const
{
	switch (command) {
	case POST:
	case GET:
	case DELETE:
	case PUT:
		return !url.empty();
	default:
		return false;
	}
}

std::string
CurlCommand::buildCommandString() const
{
	assert(isDefined());

	std::stringstream stream;

	stream << "cmd: " << CommandTabel[command] << NEWLINE;
	stream << "url: " << url << NEWLINE;
	if (!data.empty()) {
		stream << "data: " << data << NEWLINE;
	}
	if (!etag.empty()) {
		stream << "etag: " << etag << NEWLINE;
	}
	if (!authorization.empty()) {
		stream << "authorization: " << authorization << NEWLINE;
	}
	if (!content_type.empty()) {
		stream << "content_type: " << content_type << NEWLINE;
	}

	std::string str = stream.str();
	stream.str("");

	return str;
}

const char*
CurlCommand::command_cstr() const
{
	assert(command < ARRAY_SIZE(CommandTabel));

	return CommandTabel[command];
}
