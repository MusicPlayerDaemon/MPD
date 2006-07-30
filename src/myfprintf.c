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

#include "myfprintf.h"
#include "interface.h"
#include "path.h"
#include "log.h"
#include "conf.h"
#include "utils.h"

#include <stdarg.h>
#include <sys/param.h>
#include <unistd.h>
#include <sys/select.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

#define BUFFER_LENGTH	MAXPATHLEN+1024

static int myfprintf_stdLogMode = 0;
static FILE *myfprintf_out;
static FILE *myfprintf_err;
static char *myfprintf_outFilename;
static char *myfprintf_errFilename;

static void blockingWrite(const int fd, const char *string, size_t len)
{
	while (len) {
		size_t ret = xwrite(fd, string, len);
		if (ret == len)
			return;
		if (ret >= 0) {
			len -= ret;
			string += ret;
			continue;
		}
		return; /* error */
	}
}

void vfdprintf(const int fd, const char *fmt, va_list args)
{
	static char buffer[BUFFER_LENGTH + 1];
	char *buf = buffer;
	size_t len;

	vsnprintf(buf, BUFFER_LENGTH, fmt, args);
	len = strlen(buf);
	if (interfacePrintWithFD(fd, buf, len) < 0)
		blockingWrite(fd, buf, len);
}

mpd_fprintf void fdprintf(const int fd, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	vfdprintf(fd, fmt, args);
	va_end(args);
}

void myfprintfStdLogMode(FILE * out, FILE * err)
{
	myfprintf_stdLogMode = 1;
	myfprintf_out = out;
	myfprintf_err = err;
	myfprintf_outFilename = parseConfigFilePath(CONF_LOG_FILE, 1)->value;
	myfprintf_errFilename = parseConfigFilePath(CONF_ERROR_FILE, 1)->value;
}

int myfprintfCloseAndOpenLogFile(void)
{
	if (myfprintf_stdLogMode) {
		while (fclose(myfprintf_out) < 0 && errno == EINTR) ;
		while (fclose(myfprintf_err) < 0 && errno == EINTR) ;
		while ((myfprintf_out =
			fopen(myfprintf_outFilename, "a+")) == NULL
		       && errno == EINTR) ;
		if (!myfprintf_out) {
			ERROR("error re-opening log file: %s\n",
			      myfprintf_outFilename);
			return -1;
		}
		while ((myfprintf_err =
			fopen(myfprintf_errFilename, "a+")) == NULL
		       && errno == EINTR) ;
		if (!myfprintf_err) {
			ERROR("error re-opening error file: %s\n",
			      myfprintf_errFilename);
			return -1;
		}
		while (dup2(fileno(myfprintf_out), 1) < 0 && errno == EINTR) ;
		while (dup2(fileno(myfprintf_err), 2) < 0 && errno == EINTR) ;
	}

	return 0;
}

void myfprintfCloseLogFile(void)
{
	if (myfprintf_stdLogMode) {
		while (fclose(myfprintf_out) < 0 && errno == EINTR) ;
		while (fclose(myfprintf_err) < 0 && errno == EINTR) ;
	}
}
