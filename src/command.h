/* the Music Player Daemon (MPD)
 * (c)2003-2004 by Warren Dukes (shank@mercury.chem.pitt.edu
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

#include <stdio.h>

#define COMMAND_RETURN_KILL	10
#define COMMAND_RETURN_CLOSE	20

extern char * current_command;
extern int command_listNum;

int proccessListOfCommands(FILE * fp, int * permission, int * expired, 
		int listOK, List * list);

int processCommand(FILE * fp, unsigned int * permission, char * commandString);

void initCommands();

void finishCommands();

#define commandSuccess(fp)              myfprintf(fp, "OK\n")

#define commandError(fp, error, format, ... )  \
	{\
		if(current_command) { \
			myfprintf(fp, "ACK [%i@%i] {%s} " format "\n", \
					(int)error, command_listNum, \
					current_command, __VA_ARGS__); \
			current_command = NULL; \
		} \
		else { \
			myfprintf(stderr, "ACK [%i@%i] " format "\n", \
					(int)error, command_listNum, \
					__VA_ARGS__); \
		} \
	}

#endif
