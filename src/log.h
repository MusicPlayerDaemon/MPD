/* the Music Player Daemon (MPD)
 * (c)2003-2004 by Warren Dukes (shank@mercury.chem.pitt.edu)
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

#ifndef LOG_H
#define LOG_H

#include "myfprintf.h"

#define LOG_LEVEL_LOW		0
#define LOG_LEVEL_SECURE	1
#define LOG_LEVEL_DEBUG		2

extern int logLevel;

#define ERROR(x, arg...) myfprintf(stderr, x , ##arg)

#define LOG(x, arg...) myfprintf(stdout, x , ##arg)

#define SECURE(x, arg...) do { \
		if(logLevel>=LOG_LEVEL_SECURE) myfprintf(stdout, x , ##arg); \
	} while(0);
		

#define DEBUG(x, arg...) do { \
		if(logLevel>=LOG_LEVEL_DEBUG) myfprintf(stdout, x , ##arg); \
	} while(0);

void initLog();

#endif
