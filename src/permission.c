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

#include "permission.h"

#include "conf.h"
#include "list.h"
#include "log.h"

#include <string.h>

#define PERMISSION_PASSWORD_CHAR	"@"
#define PERMISSION_SEPERATOR		","

#define PERMISSION_READ_STRING		"read"
#define PERMISSION_ADD_STRING		"add"
#define PERMISSION_CONTROL_STRING	"control"
#define PERMISSION_ADMIN_STRING		"admin"

List * permission_passwords;

unsigned int permission_default;

unsigned int parsePermissions(char * string) {
	unsigned int permission = 0;
	char * temp;
	char * tok;

	if(!string) return 0;

	temp = strtok_r(string,PERMISSION_SEPERATOR,&tok);
	while(temp) {
		if(strcmp(temp,PERMISSION_READ_STRING)==0) {
			permission |= PERMISSION_READ;
		}
		else if(strcmp(temp,PERMISSION_ADD_STRING)==0) {
			permission |= PERMISSION_ADD;
		}
		else if(strcmp(temp,PERMISSION_CONTROL_STRING)==0) {
			permission |= PERMISSION_CONTROL;
		}
		else if(strcmp(temp,PERMISSION_ADMIN_STRING)==0) {
			permission |= PERMISSION_ADMIN;
		}
		else {
			ERROR("uknown permission \"%s\"\n",temp);
			exit(EXIT_FAILURE);
		}

		temp = strtok_r(NULL,PERMISSION_SEPERATOR,&tok);
	}

	return permission;
}

void initPermissions() {
	char * passwordSets;
	char * nextSet;
	char * temp;
	char * cp1;
	char * cp2;
	char * password;
	unsigned int * permission;

	permission_passwords = makeList(free);

	permission_default = PERMISSION_READ | PERMISSION_ADD | 
				PERMISSION_CONTROL | PERMISSION_ADMIN;

	if(getConf()[CONF_DEFAULT_PERMISSIONS]) {
		permission_default = parsePermissions(
				getConf()[CONF_DEFAULT_PERMISSIONS]);
	}

	if(!getConf()[CONF_PASSWORD]) return;

	if(!getConf()[CONF_DEFAULT_PERMISSIONS]) permission_default = 0;

	passwordSets = strdup(getConf()[CONF_PASSWORD]);

	nextSet = strtok_r(passwordSets,CONF_CAT_CHAR,&cp1);
	while(nextSet && strlen(nextSet)) {
		if(!strstr(nextSet,PERMISSION_PASSWORD_CHAR)) {
			ERROR("\"%s\" not found in password string \"%s\"\n",
					PERMISSION_PASSWORD_CHAR,
					nextSet);
			exit(EXIT_FAILURE);
		}

		if(!(temp = strtok_r(nextSet,PERMISSION_PASSWORD_CHAR,&cp2))) {
			ERROR("something weird just happend in permission.c\n");
			exit(EXIT_FAILURE);
		}
		password = temp;

		permission = malloc(sizeof(unsigned int));
		*permission = parsePermissions(strtok_r(NULL,"",&cp2));

		insertInList(permission_passwords,password,permission);

		nextSet = strtok_r(NULL,CONF_CAT_CHAR,&cp1);
	}

	sortList(permission_passwords);

	free(passwordSets);
}

int getPermissionFromPassword(char * password, unsigned int * permission) {
	void * foundPermission;

	if(findInList(permission_passwords,password,&foundPermission)) {
		*permission = *((unsigned int *)foundPermission);
		return 0;
	}

	return -1;
}

void finishPermissions() {
	freeList(permission_passwords);
}

unsigned int getDefaultPermissions() {
	return permission_default;
}
/* vim:set shiftwidth=4 tabstop=8 expandtab: */
