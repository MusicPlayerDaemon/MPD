#ifndef PERMISSION_H
#define PERMISSION_H

#define PERMISSION_READ		1
#define PERMISSION_ADD		2
#define PERMISSION_CONTROL	4
#define PERMISSION_ADMIN	8

void initPermissions();

int getPermissionFromPassword(char * password, unsigned int * permission);

void finishPermissions();

unsigned int getDefaultPermissions();

#endif
