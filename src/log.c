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

#include "log.h"

#include "conf.h"
#include "myfprintf.h"

#include <string.h>

int logLevel = LOG_LEVEL_LOW;

void initLog() {
	if(strcmp(getConf()[CONF_LOG_LEVEL],"default")==0) {
		if(logLevel<LOG_LEVEL_LOW) logLevel = LOG_LEVEL_LOW;
	}
	else if(strcmp(getConf()[CONF_LOG_LEVEL],"secure")==0) {
		if(logLevel<LOG_LEVEL_SECURE) logLevel = LOG_LEVEL_SECURE;
	}
	else if(strcmp(getConf()[CONF_LOG_LEVEL],"verbose")==0) {
		if(logLevel<LOG_LEVEL_DEBUG) logLevel = LOG_LEVEL_DEBUG;
	}
	else ERROR("unknown log level \"%s\"\n",getConf()[CONF_LOG_LEVEL]);
}
/* vim:set shiftwidth=4 tabstop=8 expandtab: */
