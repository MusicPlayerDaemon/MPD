/*
 * Copyright 2003-2020 The Music Player Daemon Project
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

#ifndef EXECOPEN_HXX
#define EXECOPEN_HXX

#include <stdio.h>

/*
 * like popen(3) except it doesn't use shell
 * Returns NULL on error, with errno set
 */
FILE *exec_open(int *pid, const char *cmd, const char *const args[]);

/*
 * Wait for child pid to exit
 * Returns child return code
 * On error, returns -1 and errno is set
 */
int exec_wait(int pid);

#endif
