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
			exit(-1);
		}

		temp = strtok_r(NULL,PERMISSION_SEPERATOR,&tok);
	}

	return permission;
}

void initPermissions() {
	permission_passwords = makeList(free);
	char * passwordSets;
	char * nextSet;
	char * temp;
	char * cp1;
	char * cp2;
	char * password;
	unsigned int * permission;

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
			exit(-1);
		}

		if(!(temp = strtok_r(nextSet,PERMISSION_PASSWORD_CHAR,&cp2))) {
			ERROR("something weird just happend in permission.c\n");
			exit(-1);
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
