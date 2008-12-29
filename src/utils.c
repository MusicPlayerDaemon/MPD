/* the Music Player Daemon (MPD)
 * Copyright (C) 2003-2007 by Warren Dukes (warren.dukes@gmail.com)
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

#include "utils.h"
#include "conf.h"

#include "../config.h"

#include <assert.h>
#include <string.h>
#include <sys/types.h>
#include <pwd.h>
#include <fcntl.h>
#include <glib.h>

#ifdef HAVE_IPV6
#include <sys/socket.h>
#endif

void stripReturnChar(char *string)
{
	while (string && (string = strchr(string, '\n'))) {
		*string = ' ';
	}
}

void my_usleep(long usec)
{
	struct timeval tv;

	tv.tv_sec = 0;
	tv.tv_usec = usec;

	select(0, NULL, NULL, NULL, &tv);
}

int ipv6Supported(void)
{
#ifdef HAVE_IPV6
	int s;
	s = socket(AF_INET6, SOCK_STREAM, 0);
	if (s == -1)
		return 0;
	close(s);
	return 1;
#else
	return 0;
#endif
}

G_GNUC_MALLOC char *xstrdup(const char *s)
{
	char *ret = strdup(s);
	if (G_UNLIKELY(!ret))
		g_error("OOM: strdup");
	return ret;
}

/* borrowed from git :) */

G_GNUC_MALLOC void *xmalloc(size_t size)
{
	void *ret;

	assert(G_LIKELY(size));

	ret = malloc(size);
	if (G_UNLIKELY(!ret))
		g_error("OOM: malloc");
	return ret;
}

G_GNUC_MALLOC void *xrealloc(void *ptr, size_t size)
{
	void *ret = realloc(ptr, size);

	/* some C libraries return NULL when size == 0,
	 * make sure we get a free()-able pointer (free(NULL)
	 * doesn't work with all C libraries, either) */
	if (G_UNLIKELY(!ret && !size))
		ret = realloc(ptr, 1);

	if (G_UNLIKELY(!ret))
		g_error("OOM: realloc");
	return ret;
}

G_GNUC_MALLOC void *xcalloc(size_t nmemb, size_t size)
{
	void *ret;

	assert(G_LIKELY(nmemb && size));

	ret = calloc(nmemb, size);
	if (G_UNLIKELY(!ret))
		g_error("OOM: calloc");
	return ret;
}

char *parsePath(char *path)
{
	ConfigParam *param;
	struct passwd *passwd;
	char *newPath;
	char *c;
	int foundSlash = 0;
	int pos = 1;

	if (path[0] != '/' && path[0] != '~') {
		g_warning("\"%s\" is not an absolute path", path);
		return NULL;
	} else if (path[0] == '~') {
		if (path[1] == '/' || path[1] == '\0') {
			param = getConfigParam(CONF_USER);
			if (param && param->value) {
				passwd = getpwnam(param->value);
				if (!passwd) {
					g_warning("no such user %s",
						  param->value);
					return NULL;
				}
			} else {
				passwd = getpwuid(geteuid());
				if (!passwd) {
					g_warning("problems getting passwd "
						  "entry for current user");
					return NULL;
				}
			}
		} else {
			for (c = path + 1; *c != '\0' && *c != '/'; c++);
			if (*c == '/') {
				foundSlash = 1;
				*c = '\0';
			}
			pos = c - path;

			passwd = getpwnam(path + 1);
			if (!passwd) {
				g_warning("user \"%s\" not found", path + 1);
				return NULL;
			}

			if (foundSlash)
				*c = '/';
		}

		newPath = xmalloc(strlen(passwd->pw_dir) + strlen(path + pos) + 1);
		strcpy(newPath, passwd->pw_dir);
		strcat(newPath, path + pos);
	} else {
		newPath = xstrdup(path);
	}

	return newPath;
}

int set_nonblocking(int fd)
{
	int ret, flags;

	assert(fd >= 0);

	while ((flags = fcntl(fd, F_GETFL)) < 0 && errno == EINTR) ;
	if (flags < 0)
		return flags;

	flags |= O_NONBLOCK;
	while ((ret = fcntl(fd, F_SETFL, flags)) < 0 && errno == EINTR) ;
	return ret;
}

void init_async_pipe(int file_des[2])
{
	if (pipe(file_des) < 0)
		g_error("Couldn't open pipe: %s", strerror(errno));
	if (set_nonblocking(file_des[0]) < 0)
		g_error("Couldn't set non-blocking I/O: %s", strerror(errno));
	if (set_nonblocking(file_des[1]) < 0)
		g_error("Couldn't set non-blocking I/O: %s", strerror(errno));
}

int stringFoundInStringArray(const char *const*array, const char *suffix)
{
	while (array && *array) {
		if (strcasecmp(*array, suffix) == 0)
			return 1;
		array++;
	}

	return 0;
}
