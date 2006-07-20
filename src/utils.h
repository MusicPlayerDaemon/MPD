/* the Music Player Daemon (MPD)
 * (c)2003-2006 by Warren Dukes (warren.dukes@gmail.com)
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

#ifndef UTILS_H
#define UTILS_H

#include "../config.h"

#include <stdio.h>

char *myFgets(char *buffer, int bufferSize, FILE * fp);

char *strDupToUpper(char *str);

void stripReturnChar(char *string);

void my_usleep(long usec);

int ipv6Supported();

char *appendToString(char *dest, const char *src);

unsigned long readLEuint32(const unsigned char *p);

#endif
