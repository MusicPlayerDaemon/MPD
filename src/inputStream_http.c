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
#include "inputStream_http_auth.h"

#include "utils.h"
#include "log.h"
#include "conf.h"
#include "os_compat.h"

#define HTTP_CONN_STATE_CLOSED  0
#define HTTP_CONN_STATE_INIT    1
#define HTTP_CONN_STATE_HELLO   2
#define HTTP_CONN_STATE_OPEN    3
#define HTTP_CONN_STATE_REOPEN  4

#define HTTP_BUFFER_SIZE_DEFAULT        131072
#define HTTP_PREBUFFER_SIZE_DEFAULT	(HTTP_BUFFER_SIZE_DEFAULT >> 2)

#define HTTP_REDIRECT_MAX    10

#define HTTP_MAX_TRIES 100

static char *proxyHost;
static char *proxyPort;
static char *proxyUser;
static char *proxyPassword;
static size_t bufferSize = HTTP_BUFFER_SIZE_DEFAULT;
static size_t prebufferSize = HTTP_PREBUFFER_SIZE_DEFAULT;

typedef struct _InputStreemHTTPData {
	char *host;
	char *path;
	char *port;
	int sock;
	int connState;
	char *buffer;
	size_t buflen;
	int timesRedirected;
	size_t icyMetaint;
	int prebuffer;
	size_t icyOffset;
	char *proxyAuth;
	char *httpAuth;
	/* Number of times mpd tried to get data */
	int tries;
} InputStreamHTTPData;

void inputStream_initHttp(void)
{
	ConfigParam *param = getConfigParam(CONF_HTTP_PROXY_HOST);
	char *test;
	if (param) {
		proxyHost = param->value;

		param = getConfigParam(CONF_HTTP_PROXY_PORT);

		if (!param) {
			FATAL("%s specified but not %s\n", CONF_HTTP_PROXY_HOST,
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
		long tmp = strtol(param->value, &test, 10);
		if (*test != '\0' || tmp <= 0) {
			FATAL("\"%s\" specified for %s at line %i is not a "
			      "positive integer\n",
			      param->value, CONF_HTTP_BUFFER_SIZE, param->line);
		}

		bufferSize = tmp * 1024;
	}

	param = getConfigParam(CONF_HTTP_PREBUFFER_SIZE);

	if (param) {
		long tmp = strtol(param->value, &test, 10);
		if (*test != '\0' || tmp <= 0) {
			FATAL("\"%s\" specified for %s at line %i is not a "
			      "positive integer\n",
			      param->value, CONF_HTTP_PREBUFFER_SIZE,
			      param->line);
		}

		prebufferSize = tmp * 1024;
	}

	if (prebufferSize > bufferSize)
		prebufferSize = bufferSize;
	assert(bufferSize > 0 && "http bufferSize too small");
	assert(prebufferSize > 0 && "http prebufferSize too small");
}

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
	ret->tries = 0;
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

/* returns -1 on error, 0 on success (and sets dest) */
static int my_getaddrinfo(struct addrinfo **dest,
                          const char *host, const char *port)
{
	struct addrinfo hints;
	int error;

	hints.ai_flags = 0;
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_addrlen = 0;
	hints.ai_addr = NULL;
	hints.ai_canonname = NULL;
	hints.ai_next = NULL;

	if ((error = getaddrinfo(host, port, &hints, dest))) {
		DEBUG(__FILE__ ": Error getting address info for %s:%s: %s\n",
		      host, port, gai_strerror(error));
		return -1;
	}
	return 0;
}

/* returns the fd we connected to, or -1 on error */
static int my_connect_addrs(struct addrinfo *ans)
{
	int fd;
	struct addrinfo *ap;

	/* loop through possible addresses */
	for (ap = ans; ap != NULL; ap = ap->ai_next) {
		fd = socket(ap->ai_family, ap->ai_socktype, ap->ai_protocol);
		if (fd < 0) {
			DEBUG(__FILE__ ": unable to get socket: %s\n",
			      strerror(errno));
			continue;
		}

		set_nonblocking(fd);
		if (connect(fd, ap->ai_addr, ap->ai_addrlen) >= 0
		    || errno == EINPROGRESS)
			return fd;	/* success */

		DEBUG(__FILE__ ": unable to connect: %s\n", strerror(errno));
		xclose(fd); /* failed, get the next one */
	}
	return -1;
}

static int initHTTPConnection(InputStream * inStream)
{
	struct addrinfo *ans = NULL;
	InputStreamHTTPData *data = (InputStreamHTTPData *) inStream->data;

	if ((proxyHost ? my_getaddrinfo(&ans, proxyHost, proxyPort) :
	                 my_getaddrinfo(&ans, data->host, data->port)) < 0)
		return -1;

	data->sock = my_connect_addrs(ans);
	freeaddrinfo(ans);

	if (data->sock < 0)
		return -1; /* failed */
	data->connState = HTTP_CONN_STATE_INIT;
	data->buflen = 0;
	return 0;
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
	size_t length;
	ssize_t nbytes;
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
	length = (size_t)snprintf(request, sizeof(request),
	                 "GET %s HTTP/1.1\r\n"
	                 "Host: %s\r\n"
	                 "Connection: close\r\n"
	                 "User-Agent: " PACKAGE_NAME "/" PACKAGE_VERSION "\r\n"
	                 "Range: bytes=%ld-\r\n"
	                 "%s"  /* authorization */
	                 "Icy-Metadata:1\r\n"
	                 "\r\n",
	                 data->path,
	                 data->host,
	                 inStream->offset,
	                 data->proxyAuth ? data->proxyAuth :
	                 (data->httpAuth ? data->httpAuth : ""));

	if (length >= sizeof(request))
		goto close_err;
	nbytes = write(data->sock, request, length);
	if (nbytes < 0 || (size_t)nbytes != length)
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
		if (0 == strncasecmp(cur, "\r\nContent-Length: ", 18)) {
			if (!inStream->size)
				inStream->size = atol(cur + 18);
		} else if (0 == strncasecmp(cur, "\r\nicy-metaint:", 14)) {
			data->icyMetaint = strtoul(cur + 14, NULL, 0);
		} else if (0 == strncasecmp(cur, "\r\nicy-name:", 11) ||
			   0 == strncasecmp(cur, "\r\nice-name:", 11)) {
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
		} else if (0 == strncasecmp(cur, "\r\nx-audiocast-name:", 19)) {
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
		} else if (0 == strncasecmp(cur, "\r\nContent-Type:", 15)) {
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
	char *r = NULL;
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
	size_t tosend = 0;
	size_t inlen = size * nmemb;
	size_t maxToSend = data->buflen;

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
			size_t metalen = *(data->buffer);
			/* maybe we're in some strange universe where a byte
			 * can hold more than 255 ... */
			assert(metalen <= 255 && "metalen greater than 255");
			metalen <<= 4;
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
		assert(data->icyOffset <= data->icyMetaint &&
		       "icyOffset bigger than icyMetaint!");
		maxToSend = data->icyMetaint - data->icyOffset;
		maxToSend = maxToSend > data->buflen ? data->buflen : maxToSend;
	}

	if (data->buflen > 0) {
		tosend = inlen > maxToSend ? maxToSend : inlen;
		tosend = (tosend / size) * size;

		memcpy(ptr, data->buffer, tosend);
		data->buflen -= tosend;
		data->icyOffset += tosend;
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
			      bufferSize - 1 - data->buflen);
		/*
		 * If the connection is currently unavailable, or
		 * interrupted (EINTR)
		 * Don't give an error, so it's retried later.
		 * Max times in a row to retry this is HTTP_MAX_TRIES
		 */
		if (readed < 0 &&
		    (errno == EAGAIN || errno == EINTR) &&
		    data->tries < HTTP_MAX_TRIES) {
			data->tries++;
			DEBUG(__FILE__": Resource unavailable, trying %i "
			      "times again\n", HTTP_MAX_TRIES - data->tries);
			readed = 0;
		} else if (readed <= 0) {
			while (close(data->sock) && errno == EINTR);
			data->connState = HTTP_CONN_STATE_CLOSED;
			readed = 0;
		} else /* readed > 0, reset */
			data->tries = 0;

		data->buflen += readed;
	}

	if (data->buflen > prebufferSize)
		data->prebuffer = 0;

	return (readed ? 1 : 0);
}
