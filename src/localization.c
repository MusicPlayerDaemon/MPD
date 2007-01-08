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

#include "localization.h"
#include "utils.h"

#include <stdlib.h>

#ifdef HAVE_LOCALE
#ifdef HAVE_LANGINFO_CODESET
#include <locale.h>
#include <langinfo.h>
#endif
#endif

static char *localeCharset = NULL;

void setLocaleCharset(char *charset)
{
	if (localeCharset)
		free(localeCharset);
	localeCharset = charset;
}

char *getLocaleCharset(void)
{
	return localeCharset;
}

void initLocalization(void)
{
#ifdef HAVE_LOCALE
#ifdef HAVE_LANGINFO_CODESET
	char *temp;
	char *originalLocale;
	char *currentLocale;

	if (!(originalLocale = setlocale(LC_CTYPE, NULL))) {
		WARNING("problems getting locale with setlocale()\n");
	} else {
		originalLocale = xstrdup(originalLocale);

		if (!(currentLocale = setlocale(LC_CTYPE, ""))) {
			WARNING("problems setting current locale with "
			        "setlocale()\n");
		} else {
			if (strcmp(currentLocale, "C") == 0 ||
			    strcmp(currentLocale, "POSIX") == 0) {
				WARNING("current locale is \"%s\"\n",
				        currentLocale);
				setLocaleCharset(xstrdup(""));
			} else if ((temp = nl_langinfo(CODESET))) {
				setLocaleCharset(xstrdup(temp));
			} else {
				WARNING("problems getting charset for "
				        "locale\n");
			}

			if (!setlocale(LC_CTYPE, originalLocale)) {
				WARNING("problems resetting locale with "
				        "setlocale()\n");
			}
		}

		free(originalLocale);
	}
#endif
#endif
}

void finishLocalization(void)
{
	setLocaleCharset(NULL);
}
