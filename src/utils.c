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
#include "log.h"
#include "conf.h"

#include "../config.h"

#include <assert.h>
#include <string.h>
#include <sys/types.h>
#include <pwd.h>
#include <fcntl.h>

#ifdef HAVE_IPV6
#include <sys/socket.h>
#endif

char *myFgets(char *buffer, int bufferSize, FILE * fp)
{
	char *ret = fgets(buffer, bufferSize, fp);
	if (ret && strlen(buffer) > 0 && buffer[strlen(buffer) - 1] == '\n') {
		buffer[strlen(buffer) - 1] = '\0';
	}
	if (ret && strlen(buffer) > 0 && buffer[strlen(buffer) - 1] == '\r') {
		buffer[strlen(buffer) - 1] = '\0';
	}
	return ret;
}

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

char *appendToString(char *dest, const char *src)
{
	int destlen;
	int srclen = strlen(src);

	if (dest == NULL) {
		dest = xmalloc(srclen + 1);
		memset(dest, 0, srclen + 1);
		destlen = 0;
	} else {
		destlen = strlen(dest);
		dest = xrealloc(dest, destlen + srclen + 1);
	}

	memcpy(dest + destlen, src, srclen);
	dest[destlen + srclen] = '\0';

	return dest;
}

unsigned long readLEuint32(const unsigned char *p)
{
	return ((unsigned long)p[0] << 0) |
	    ((unsigned long)p[1] << 8) |
	    ((unsigned long)p[2] << 16) | ((unsigned long)p[3] << 24);
}

mpd_malloc char *xstrdup(const char *s)
{
	char *ret = strdup(s);
	if (mpd_unlikely(!ret))
		FATAL("OOM: strdup\n");
	return ret;
}

/* borrowed from git :) */

mpd_malloc void *xmalloc(size_t size)
{
	void *ret;

	assert(mpd_likely(size));

	ret = malloc(size);
	if (mpd_unlikely(!ret))
		FATAL("OOM: malloc\n");
	return ret;
}

mpd_malloc void *xrealloc(void *ptr, size_t size)
{
	void *ret = realloc(ptr, size);

	/* some C libraries return NULL when size == 0,
	 * make sure we get a free()-able pointer (free(NULL)
	 * doesn't work with all C libraries, either) */
	if (mpd_unlikely(!ret && !size))
		ret = realloc(ptr, 1);

	if (mpd_unlikely(!ret))
		FATAL("OOM: realloc\n");
	return ret;
}

mpd_malloc void *xcalloc(size_t nmemb, size_t size)
{
	void *ret;

	assert(mpd_likely(nmemb && size));

	ret = calloc(nmemb, size);
	if (mpd_unlikely(!ret))
		FATAL("OOM: calloc\n");
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
		ERROR("\"%s\" is not an absolute path\n", path);
		return NULL;
	} else if (path[0] == '~') {
		if (path[1] == '/' || path[1] == '\0') {
			param = getConfigParam(CONF_USER);
			if (param && param->value) {
				passwd = getpwnam(param->value);
				if (!passwd) {
					ERROR("no such user %s\n",
				              param->value);
					return NULL;
				}
			} else {
				passwd = getpwuid(geteuid());
				if (!passwd) {
					ERROR("problems getting passwd entry "
					      "for current user\n");
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
				ERROR("user \"%s\" not found\n", path + 1);
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
		FATAL("Couldn't open pipe: %s", strerror(errno));
	if (set_nonblocking(file_des[0]) < 0)
		FATAL("Couldn't set non-blocking I/O: %s\n", strerror(errno));
	if (set_nonblocking(file_des[1]) < 0)
		FATAL("Couldn't set non-blocking I/O: %s\n", strerror(errno));
}

void xpthread_mutex_init(pthread_mutex_t *m, const pthread_mutexattr_t *a)
{
	int err;
	if ((err = pthread_mutex_init(m, a)))
		FATAL("failed to init mutex: %s\n", strerror(err));
}

void xpthread_cond_init(pthread_cond_t *c, pthread_condattr_t *a)
{
	int err;
	if ((err = pthread_cond_init(c, a)))
		FATAL("failed to init cond: %s\n", strerror(err));
}

void xpthread_mutex_destroy(pthread_mutex_t *mutex)
{
	int err;
	if ((err = pthread_mutex_destroy(mutex)))
		FATAL("failed to destroy mutex: %s\n", strerror(err));
}

void xpthread_cond_destroy(pthread_cond_t *cond)
{
	int err;
	if ((err = pthread_cond_destroy(cond)))
		FATAL("failed to destroy cond: %s\n", strerror(err));
}

int prefixcmp(const char *str, const char *prefix)
{
	for (; ; str++, prefix++)
		if (!*prefix)
			return 0;
		else if (*str != *prefix)
			return (unsigned char)*prefix - (unsigned char)*str;
}

