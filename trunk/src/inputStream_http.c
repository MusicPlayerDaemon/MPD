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

#include "inputStream_http.h"

#include "utils.h"
#include "log.h"
#include "conf.h"

#include <stdio.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/param.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

#define HTTP_CONN_STATE_CLOSED  0
#define HTTP_CONN_STATE_INIT    1
#define HTTP_CONN_STATE_HELLO   2
#define HTTP_CONN_STATE_OPEN    3
#define HTTP_CONN_STATE_REOPEN  4

#define HTTP_BUFFER_SIZE_DEFAULT        131072
#define HTTP_PREBUFFER_SIZE_DEFAULT	(HTTP_BUFFER_SIZE_DEFAULT >> 2)

#define HTTP_REDIRECT_MAX    10

static char *proxyHost;
static char *proxyPort;
static char *proxyUser;
static char *proxyPassword;
static int bufferSize = HTTP_BUFFER_SIZE_DEFAULT;
static int prebufferSize = HTTP_PREBUFFER_SIZE_DEFAULT;

typedef struct _InputStreemHTTPData {
	char *host;
	char *path;
	char *port;
	int sock;
	int connState;
	char *buffer;
	size_t buflen;
	int timesRedirected;
	int icyMetaint;
	int prebuffer;
	int icyOffset;
	char *proxyAuth;
	char *httpAuth;
} InputStreamHTTPData;

void inputStream_initHttp(void)
{
	ConfigParam *param = getConfigParam(CONF_HTTP_PROXY_HOST);
	char *test;

	if (param) {
		proxyHost = param->value;

		param = getConfigParam(CONF_HTTP_PROXY_PORT);

		if (!param) {
			FATAL("%s specified but not %s", CONF_HTTP_PROXY_HOST,
			      CONF_HTTP_PROXY_PORT);
		}
		proxyPort = param->value;

		param = getConfigParam(CONF_HTTP_PROXY_USER);

		if (param) {
			proxyUser = param->value;

			param = getConfigParam(CONF_HTTP_PROXY_PASSWORD);

			if (!param) {
				FATAL("%s specified but not %s\n",
				      CONF_HTTP_PROXY_USER,
				      CONF_HTTP_PROXY_PASSWORD);
			}

			proxyPassword = param->value;
		} else {
			param = getConfigParam(CONF_HTTP_PROXY_PASSWORD);

			if (param) {
				FATAL("%s specified but not %s\n",
				      CONF_HTTP_PROXY_PASSWORD, CONF_HTTP_PROXY_USER);
			}
		}
	} else if ((param = getConfigParam(CONF_HTTP_PROXY_PORT))) {
		FATAL("%s specified but not %s, line %i\n",
		      CONF_HTTP_PROXY_PORT, CONF_HTTP_PROXY_HOST, param->line);
	} else if ((param = getConfigParam(CONF_HTTP_PROXY_USER))) {
		FATAL("%s specified but not %s, line %i\n",
		      CONF_HTTP_PROXY_USER, CONF_HTTP_PROXY_HOST, param->line);
	} else if ((param = getConfigParam(CONF_HTTP_PROXY_PASSWORD))) {
		FATAL("%s specified but not %s, line %i\n",
		      CONF_HTTP_PROXY_PASSWORD, CONF_HTTP_PROXY_HOST,
		      param->line);
	}

	param = getConfigParam(CONF_HTTP_BUFFER_SIZE);

	if (param) {
		bufferSize = strtol(param->value, &test, 10);

		if (bufferSize <= 0 || *test != '\0') {
			FATAL("\"%s\" specified for %s at line %i is not a "
			      "positive integer\n",
			      param->value, CONF_HTTP_BUFFER_SIZE, param->line);
		}

		bufferSize *= 1024;

		if (prebufferSize > bufferSize)
			prebufferSize = bufferSize;
	}

	param = getConfigParam(CONF_HTTP_PREBUFFER_SIZE);

	if (param) {
		prebufferSize = strtol(param->value, &test, 10);

		if (prebufferSize <= 0 || *test != '\0') {
			FATAL("\"%s\" specified for %s at line %i is not a "
			      "positive integer\n",
			      param->value, CONF_HTTP_PREBUFFER_SIZE,
			      param->line);
		}

		prebufferSize *= 1024;
	}

	if (prebufferSize > bufferSize)
		prebufferSize = bufferSize;
}

/* base64 code taken from xmms */

#define BASE64_LENGTH(len) (4 * (((len) + 2) / 3))

static char *base64Dup(char *s)
{
	int i;
	int len = strlen(s);
	char *ret = xcalloc(BASE64_LENGTH(len) + 1, 1);
	unsigned char *p = (unsigned char *)ret;

	char tbl[64] = {
		'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
		'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
		'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
		'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
		'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
		'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
		'w', 'x', 'y', 'z', '0', '1', '2', '3',
		'4', '5', '6', '7', '8', '9', '+', '/'
	};

	/* Transform the 3x8 bits to 4x6 bits, as required by base64.  */
	for (i = 0; i < len; i += 3) {
		*p++ = tbl[s[0] >> 2];
		*p++ = tbl[((s[0] & 3) << 4) + (s[1] >> 4)];
		*p++ = tbl[((s[1] & 0xf) << 2) + (s[2] >> 6)];
		*p++ = tbl[s[2] & 0x3f];
		s += 3;
	}
	/* Pad the result if necessary...  */
	if (i == len + 1)
		*(p - 1) = '=';
	else if (i == len + 2)
		*(p - 1) = *(p - 2) = '=';
	/* ...and zero-terminate it.  */
	*p = '\0';

	return ret;
}

static char *authString(char *header, char *user, char *password)
{
	char *ret = NULL;
	int templen;
	char *temp;
	char *temp64;

	if (!user || !password)
		return NULL;

	templen = strlen(user) + strlen(password) + 2;
	temp = xmalloc(templen);
	strcpy(temp, user);
	strcat(temp, ":");
	strcat(temp, password);
	temp64 = base64Dup(temp);
	free(temp);

	ret = xmalloc(strlen(temp64) + strlen(header) + 3);
	strcpy(ret, header);
	strcat(ret, temp64);
	strcat(ret, "\r\n");
	free(temp64);

	return ret;
}

#define PROXY_AUTH_HEADER	"Proxy-Authorization: Basic "
#define HTTP_AUTH_HEADER	"Authorization: Basic "

#define proxyAuthString(x, y)	authString(PROXY_AUTH_HEADER, x, y)
#define httpAuthString(x, y)	authString(HTTP_AUTH_HEADER, x, y)

static InputStreamHTTPData *newInputStreamHTTPData(void)
{
	InputStreamHTTPData *ret = xmalloc(sizeof(InputStreamHTTPData));

	if (proxyHost) {
		ret->proxyAuth = proxyAuthString(proxyUser, proxyPassword);
	} else
		ret->proxyAuth = NULL;

	ret->httpAuth = NULL;
	ret->host = NULL;
	ret->path = NULL;
	ret->port = NULL;
	ret->connState = HTTP_CONN_STATE_CLOSED;
	ret->timesRedirected = 0;
	ret->icyMetaint = 0;
	ret->prebuffer = 0;
	ret->icyOffset = 0;
	ret->buffer = xmalloc(bufferSize);

	return ret;
}

static void freeInputStreamHTTPData(InputStreamHTTPData * data)
{
	if (data->host)
		free(data->host);
	if (data->path)
		free(data->path);
	if (data->port)
		free(data->port);
	if (data->proxyAuth)
		free(data->proxyAuth);
	if (data->httpAuth)
		free(data->httpAuth);

	free(data->buffer);

	free(data);
}

static int parseUrl(InputStreamHTTPData * data, char *url)
{
	char *temp;
	char *colon;
	char *slash;
	char *at;
	int len;

	if (strncmp("http://", url, strlen("http://")) != 0)
		return -1;

	temp = url + strlen("http://");

	colon = strchr(temp, ':');
	at = strchr(temp, '@');

	if (data->httpAuth) {
		free(data->httpAuth);
		data->httpAuth = NULL;
	}

	if (at) {
		char *user;
		char *passwd;

		if (colon && colon < at) {
			user = xmalloc(colon - temp + 1);
			memcpy(user, temp, colon - temp);
			user[colon - temp] = '\0';

			passwd = xmalloc(at - colon);
			memcpy(passwd, colon + 1, at - colon - 1);
			passwd[at - colon - 1] = '\0';
		} else {
			user = xmalloc(at - temp + 1);
			memcpy(user, temp, at - temp);
			user[at - temp] = '\0';

			passwd = xstrdup("");
		}

		data->httpAuth = httpAuthString(user, passwd);

		free(user);
		free(passwd);

		temp = at + 1;
		colon = strchr(temp, ':');
	}

	slash = strchr(temp, '/');

	if (slash && colon && slash <= colon)
		return -1;

	/* fetch the host portion */
	if (colon)
		len = colon - temp + 1;
	else if (slash)
		len = slash - temp + 1;
	else
		len = strlen(temp) + 1;

	if (len <= 1)
		return -1;

	data->host = xmalloc(len);
	memcpy(data->host, temp, len - 1);
	data->host[len - 1] = '\0';
	/* fetch the port */
	if (colon && (!slash || slash != colon + 1)) {
		len = strlen(colon) - 1;
		if (slash)
			len -= strlen(slash);
		data->port = xmalloc(len + 1);
		memcpy(data->port, colon + 1, len);
		data->port[len] = '\0';
		DEBUG(__FILE__ ": Port: %s\n", data->port);
	} else {
		data->port = xstrdup("80");
	}

	/* fetch the path */
	if (proxyHost)
		data->path = xstrdup(url);
	else
		data->path = xstrdup(slash ? slash : "/");

	return 0;
}

static int initHTTPConnection(InputStream * inStream)
{
	char *connHost;
	char *connPort;
	struct addrinfo *ans = NULL;
	struct addrinfo *ap = NULL;
	struct addrinfo hints;
	int error, flags;
	InputStreamHTTPData *data = (InputStreamHTTPData *) inStream->data;
	/**
	 * Setup hints
	 */
	hints.ai_flags = 0;
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_addrlen = 0;
	hints.ai_addr = NULL;
	hints.ai_canonname = NULL;
	hints.ai_next = NULL;

	if (proxyHost) {
		connHost = proxyHost;
		connPort = proxyPort;
	} else {
		connHost = data->host;
		connPort = data->port;
	}

	error = getaddrinfo(connHost, connPort, &hints, &ans);
	if (error) {
		DEBUG(__FILE__ ": Error getting address info: %s\n",
		      gai_strerror(error));
		return -1;
	}

	/* loop through possible addresses */
	for (ap = ans; ap != NULL; ap = ap->ai_next) {
		if ((data->sock = socket(ap->ai_family, ap->ai_socktype,
					 ap->ai_protocol)) < 0) {
			DEBUG(__FILE__ ": unable to connect: %s\n",
			      strerror(errno));
			freeaddrinfo(ans);
			return -1;
		}

		flags = fcntl(data->sock, F_GETFL, 0);
		fcntl(data->sock, F_SETFL, flags | O_NONBLOCK);

		if (connect(data->sock, ap->ai_addr, ap->ai_addrlen) >= 0
		    || errno == EINPROGRESS) {
			data->connState = HTTP_CONN_STATE_INIT;
			data->buflen = 0;
			freeaddrinfo(ans);
			return 0;	/* success */
		}

		/* failed, get the next one */

		DEBUG(__FILE__ ": unable to connect: %s\n", strerror(errno));
		close(data->sock);
	}

	freeaddrinfo(ans);
	return -1;	/* failed */
}

static int finishHTTPInit(InputStream * inStream)
{
	InputStreamHTTPData *data = (InputStreamHTTPData *) inStream->data;
	struct timeval tv;
	fd_set writeSet;
	fd_set errorSet;
	int error;
	socklen_t error_len = sizeof(int);
	int ret;
	int length;
	char request[2048];

	tv.tv_sec = 0;
	tv.tv_usec = 0;

	FD_ZERO(&writeSet);
	FD_ZERO(&errorSet);
	FD_SET(data->sock, &writeSet);
	FD_SET(data->sock, &errorSet);

	ret = select(data->sock + 1, NULL, &writeSet, &errorSet, &tv);

	if (ret == 0 || (ret < 0 && errno == EINTR))
		return 0;

	if (ret < 0) {
		DEBUG(__FILE__ ": problem select'ing: %s\n", strerror(errno));
		goto close_err;
	}

	getsockopt(data->sock, SOL_SOCKET, SO_ERROR, &error, &error_len);
	if (error)
		goto close_err;

	/* deal with ICY metadata later, for now its fucking up stuff! */
	length = snprintf(request, sizeof(request),
	                 "GET %s HTTP/1.1\r\n" "Host: %s\r\n"
			 /*"Connection: close\r\n" */
			 "User-Agent: %s/%s\r\n"
			 "Range: bytes=%ld-\r\n"
			 "%s"	/* authorization */
			 "Icy-Metadata:1\r\n"
			 "\r\n",
			 data->path, data->host,
			 PACKAGE_NAME, PACKAGE_VERSION,
			 inStream->offset,
			 data->proxyAuth ? data->proxyAuth :
			  (data->httpAuth ? data->httpAuth : ""));

	if (length >= sizeof(request))
		goto close_err;
	ret = write(data->sock, request, length);
	if (ret != length)
		goto close_err;

	data->connState = HTTP_CONN_STATE_HELLO;
	return 0;

close_err:
	close(data->sock);
	data->connState = HTTP_CONN_STATE_CLOSED;
	return -1;
}

static int getHTTPHello(InputStream * inStream)
{
	InputStreamHTTPData *data = (InputStreamHTTPData *) inStream->data;
	fd_set readSet;
	struct timeval tv;
	int ret;
	char *needle;
	char *cur = data->buffer;
	int rc;
	long readed;

	FD_ZERO(&readSet);
	FD_SET(data->sock, &readSet);

	tv.tv_sec = 0;
	tv.tv_usec = 0;

	ret = select(data->sock + 1, &readSet, NULL, NULL, &tv);

	if (ret == 0 || (ret < 0 && errno == EINTR))
		return 0;

	if (ret < 0) {
		data->connState = HTTP_CONN_STATE_CLOSED;
		close(data->sock);
		data->buflen = 0;
		return -1;
	}

	if (data->buflen >= bufferSize - 1) {
		data->connState = HTTP_CONN_STATE_CLOSED;
		close(data->sock);
		return -1;
	}

	readed = recv(data->sock, data->buffer + data->buflen,
		      bufferSize - 1 - data->buflen, 0);

	if (readed < 0 && (errno == EAGAIN || errno == EINTR))
		return 0;

	if (readed <= 0) {
		data->connState = HTTP_CONN_STATE_CLOSED;
		close(data->sock);
		data->buflen = 0;
		return -1;
	}

	data->buffer[data->buflen + readed] = '\0';
	data->buflen += readed;

	needle = strstr(data->buffer, "\r\n\r\n");

	if (!needle)
		return 0;

	if (0 == strncmp(cur, "HTTP/1.0 ", 9)) {
		inStream->seekable = 0;
		rc = atoi(cur + 9);
	} else if (0 == strncmp(cur, "HTTP/1.1 ", 9)) {
		inStream->seekable = 1;
		rc = atoi(cur + 9);
	} else if (0 == strncmp(cur, "ICY 200 OK", 10)) {
		inStream->seekable = 0;
		rc = 200;
	} else if (0 == strncmp(cur, "ICY 400 Server Full", 19))
		rc = 400;
	else if (0 == strncmp(cur, "ICY 404", 7))
		rc = 404;
	else {
		close(data->sock);
		data->connState = HTTP_CONN_STATE_CLOSED;
		return -1;
	}

	switch (rc) {
	case 200:
	case 206:
		break;
	case 301:
	case 302:
		cur = strstr(cur, "Location: ");
		if (cur) {
			char *url;
			int curlen = 0;
			cur += strlen("Location: ");
			while (*(cur + curlen) != '\0'
			       && *(cur + curlen) != '\r') {
				curlen++;
			}
			url = xmalloc(curlen + 1);
			memcpy(url, cur, curlen);
			url[curlen] = '\0';
			ret = parseUrl(data, url);
			free(url);
			if (ret == 0 && data->timesRedirected <
			    HTTP_REDIRECT_MAX) {
				data->timesRedirected++;
				close(data->sock);
				data->connState = HTTP_CONN_STATE_REOPEN;
				data->buflen = 0;
				return 0;
			}
		}
	case 400:
	case 401:
	case 403:
	case 404:
	default:
		close(data->sock);
		data->connState = HTTP_CONN_STATE_CLOSED;
		data->buflen = 0;
		return -1;
	}

	cur = strstr(data->buffer, "\r\n");
	while (cur && cur != needle) {
		if (0 == strncmp(cur, "\r\nContent-Length: ", 18)) {
			if (!inStream->size)
				inStream->size = atol(cur + 18);
		} else if (0 == strncmp(cur, "\r\nicy-metaint:", 14)) {
			data->icyMetaint = atoi(cur + 14);
		} else if (0 == strncmp(cur, "\r\nicy-name:", 11) ||
			   0 == strncmp(cur, "\r\nice-name:", 11)) {
			int incr = 11;
			char *temp = strstr(cur + incr, "\r\n");
			if (!temp)
				break;
			*temp = '\0';
			if (inStream->metaName)
				free(inStream->metaName);
			while (*(incr + cur) == ' ')
				incr++;
			inStream->metaName = xstrdup(cur + incr);
			*temp = '\r';
			DEBUG("inputStream_http: metaName: %s\n",
			      inStream->metaName);
		} else if (0 == strncmp(cur, "\r\nx-audiocast-name:", 19)) {
			int incr = 19;
			char *temp = strstr(cur + incr, "\r\n");
			if (!temp)
				break;
			*temp = '\0';
			if (inStream->metaName)
				free(inStream->metaName);
			while (*(incr + cur) == ' ')
				incr++;
			inStream->metaName = xstrdup(cur + incr);
			*temp = '\r';
			DEBUG("inputStream_http: metaName: %s\n",
			      inStream->metaName);
		} else if (0 == strncmp(cur, "\r\nContent-Type:", 15)) {
			int incr = 15;
			char *temp = strstr(cur + incr, "\r\n");
			if (!temp)
				break;
			*temp = '\0';
			if (inStream->mime)
				free(inStream->mime);
			while (*(incr + cur) == ' ')
				incr++;
			inStream->mime = xstrdup(cur + incr);
			*temp = '\r';
		}

		cur = strstr(cur + 2, "\r\n");
	}

	if (inStream->size <= 0)
		inStream->seekable = 0;

	needle += 4;	/* 4 == strlen("\r\n\r\n") */
	data->buflen -= (needle - data->buffer);
	/*fwrite(data->buffer, 1, data->buflen, stdout); */
	memmove(data->buffer, needle, data->buflen);

	data->connState = HTTP_CONN_STATE_OPEN;

	data->prebuffer = 1;

	return 0;
}

int inputStream_httpOpen(InputStream * inStream, char *url)
{
	InputStreamHTTPData *data = newInputStreamHTTPData();

	inStream->data = data;

	if (parseUrl(data, url) < 0) {
		freeInputStreamHTTPData(data);
		return -1;
	}

	if (initHTTPConnection(inStream) < 0) {
		freeInputStreamHTTPData(data);
		return -1;
	}

	inStream->seekFunc = inputStream_httpSeek;
	inStream->closeFunc = inputStream_httpClose;
	inStream->readFunc = inputStream_httpRead;
	inStream->atEOFFunc = inputStream_httpAtEOF;
	inStream->bufferFunc = inputStream_httpBuffer;

	return 0;
}

int inputStream_httpSeek(InputStream * inStream, long offset, int whence)
{
	InputStreamHTTPData *data;

	if (!inStream->seekable)
		return -1;

	switch (whence) {
	case SEEK_SET:
		inStream->offset = offset;
		break;
	case SEEK_CUR:
		inStream->offset += offset;
		break;
	case SEEK_END:
		inStream->offset = inStream->size + offset;
		break;
	default:
		return -1;
	}

	data = (InputStreamHTTPData *)inStream->data;
	close(data->sock);
	data->connState = HTTP_CONN_STATE_REOPEN;
	data->buflen = 0;

	inputStream_httpBuffer(inStream);

	return 0;
}

static void parseIcyMetadata(InputStream * inStream, char *metadata, int size)
{
	char *r;
	char *s;
	char *temp = xmalloc(size + 1);
	memcpy(temp, metadata, size);
	temp[size] = '\0';
	s = strtok_r(temp, ";", &r);
	while (s) {
		if (0 == strncmp(s, "StreamTitle=", 12)) {
			int cur = 12;
			if (inStream->metaTitle)
				free(inStream->metaTitle);
			if (*(s + cur) == '\'')
				cur++;
			if (s[strlen(s) - 1] == '\'') {
				s[strlen(s) - 1] = '\0';
			}
			inStream->metaTitle = xstrdup(s + cur);
			DEBUG("inputStream_http: metaTitle: %s\n",
			      inStream->metaTitle);
		}
		s = strtok_r(NULL, ";", &r);
	}
	free(temp);
}

size_t inputStream_httpRead(InputStream * inStream, void *ptr, size_t size,
			    size_t nmemb)
{
	InputStreamHTTPData *data = (InputStreamHTTPData *) inStream->data;
	long tosend = 0;
	long inlen = size * nmemb;
	long maxToSend = data->buflen;

	inputStream_httpBuffer(inStream);

	switch (data->connState) {
	case HTTP_CONN_STATE_OPEN:
		if (data->prebuffer || data->buflen < data->icyMetaint)
			return 0;

		break;
	case HTTP_CONN_STATE_CLOSED:
		if (data->buflen)
			break;
	default:
		return 0;
	}

	if (data->icyMetaint > 0) {
		if (data->icyOffset >= data->icyMetaint) {
			int metalen = *(data->buffer);
			metalen <<= 4;
			if (metalen < 0)
				metalen = 0;
			if (metalen + 1 > data->buflen) {
				/* damn that's some fucking big metadata! */
				if (bufferSize < metalen + 1) {
					data->connState =
					    HTTP_CONN_STATE_CLOSED;
					close(data->sock);
					data->buflen = 0;
				}
				return 0;
			}
			if (metalen > 0) {
				parseIcyMetadata(inStream, data->buffer + 1,
						 metalen);
			}
			data->buflen -= metalen + 1;
			memmove(data->buffer, data->buffer + metalen + 1,
				data->buflen);
			data->icyOffset = 0;
		}
		maxToSend = data->icyMetaint - data->icyOffset;
		maxToSend = maxToSend > data->buflen ? data->buflen : maxToSend;
	}

	if (data->buflen > 0) {
		tosend = inlen > maxToSend ? maxToSend : inlen;
		tosend = (tosend / size) * size;

		memcpy(ptr, data->buffer, tosend);
		/*fwrite(ptr,1,readed,stdout); */
		data->buflen -= tosend;
		data->icyOffset += tosend;
		/*fwrite(data->buffer,1,readed,stdout); */
		memmove(data->buffer, data->buffer + tosend, data->buflen);

		inStream->offset += tosend;
	}

	return tosend / size;
}

int inputStream_httpClose(InputStream * inStream)
{
	InputStreamHTTPData *data = (InputStreamHTTPData *) inStream->data;

	switch (data->connState) {
	case HTTP_CONN_STATE_CLOSED:
		break;
	default:
		close(data->sock);
	}

	freeInputStreamHTTPData(data);

	return 0;
}

int inputStream_httpAtEOF(InputStream * inStream)
{
	InputStreamHTTPData *data = (InputStreamHTTPData *) inStream->data;
	switch (data->connState) {
	case HTTP_CONN_STATE_CLOSED:
		if (data->buflen == 0)
			return 1;
	default:
		return 0;
	}
}

int inputStream_httpBuffer(InputStream * inStream)
{
	InputStreamHTTPData *data = (InputStreamHTTPData *) inStream->data;
	ssize_t readed = 0;

	if (data->connState == HTTP_CONN_STATE_REOPEN) {
		if (initHTTPConnection(inStream) < 0)
			return -1;
	}

	if (data->connState == HTTP_CONN_STATE_INIT) {
		if (finishHTTPInit(inStream) < 0)
			return -1;
	}

	if (data->connState == HTTP_CONN_STATE_HELLO) {
		if (getHTTPHello(inStream) < 0)
			return -1;
	}

	switch (data->connState) {
	case HTTP_CONN_STATE_OPEN:
	case HTTP_CONN_STATE_CLOSED:
		break;
	default:
		return -1;
	}

	if (data->buflen == 0 || data->buflen < data->icyMetaint) {
		data->prebuffer = 1;
	} else if (data->buflen > prebufferSize)
		data->prebuffer = 0;

	if (data->connState == HTTP_CONN_STATE_OPEN &&
	    data->buflen < bufferSize - 1) {
		readed = read(data->sock, data->buffer + data->buflen,
			      (size_t) (bufferSize - 1 - data->buflen));

		if (readed < 0 && (errno == EAGAIN || errno == EINTR)) {
			readed = 0;
		} else if (readed <= 0) {
			close(data->sock);
			data->connState = HTTP_CONN_STATE_CLOSED;
			readed = 0;
		}
		/*fwrite(data->buffer+data->buflen,1,readed,stdout); */
		data->buflen += readed;
	}

	if (data->buflen > prebufferSize)
		data->prebuffer = 0;

	return (readed ? 1 : 0);
}
