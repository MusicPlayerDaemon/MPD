/* the Music Player Daemon (MPD)
 * (c)2003-2006 by Warren Dukes (warren.dukes@gmail.com
 * This project's homepage is: http://www.musicpd.org
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef COMMAND_H
#define COMMAND_H

#include "../config.h"

#include "list.h"
#include "myfprintf.h"
#include "log.h"
#include "ack.h"
#include "sllist.h"

#include <unistd.h>
#include <stdio.h>

#define COMMAND_RETURN_KILL	10
#define COMMAND_RETURN_CLOSE	20
#define COMMAND_MASTER_READY	30

extern char *current_command;
extern int command_listNum;

int processListOfCommands(int fd, int *permission, int *expired,
			  int listOK, struct strnode *list);

int processCommand(int fd, int *permission, char *commandString);

void initCommands();

void finishCommands();

#define commandSuccess(fd)              fdprintf(fd, "OK\n")

#define commandError(fd, error, format, ... ) do \
	{\
		if (current_command) { \
			fdprintf(fd, "ACK [%i@%i] {%s} " format "\n", \
					(int)error, command_listNum, \
					current_command, __VA_ARGS__); \
			current_command = NULL; \
		} \
		else { \
			fdprintf(STDERR_FILENO, "ACK [%i@%i] " format "\n", \
					(int)error, command_listNum, \
					__VA_ARGS__); \
		} \
	} while (0)
#endif
