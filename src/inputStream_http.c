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
#include "ringbuf.h"
#include "condition.h"

enum conn_state { /* only written by io thread, read by both */
	CONN_STATE_NEW,             /* just (re)initialized */
	CONN_STATE_REDIRECT,        /* redirect */
	CONN_STATE_CONNECTED,       /* connected to the socket */
	CONN_STATE_REQUESTED,       /* sent HTTP request */
	CONN_STATE_RESP_HEAD,       /* reading HTTP response header */
	CONN_STATE_PREBUFFER,       /* prebuffering data stream */
	CONN_STATE_BUFFER,          /* buffering data stream */
	CONN_STATE_BUFFER_FULL,     /* reading actual data stream */
	CONN_STATE_CLOSED           /* it's over, time to die */
};

/* used by all HTTP header matching */
#define match(s) !strncasecmp(cur, s, (offset = sizeof(s) - 1))

#define assert_state(st) assert(data->state == st)
#define assert_state2(s1,s2) assert((data->state == s1) || (data->state == s2))

enum conn_action { /* only written by control thread, read by both */
	CONN_ACTION_NONE,
	CONN_ACTION_CLOSE,
	CONN_ACTION_DOSEEK
};

#define HTTP_BUFFER_SIZE_DEFAULT        131072
#define HTTP_PREBUFFER_SIZE_DEFAULT	(HTTP_BUFFER_SIZE_DEFAULT >> 2)
#define HTTP_REDIRECT_MAX    10

static char *proxy_host;
static char *proxy_port;
static char *proxy_user;
static char *proxy_password;
static size_t buffer_size = HTTP_BUFFER_SIZE_DEFAULT;
static size_t prebuffer_size = HTTP_PREBUFFER_SIZE_DEFAULT;

struct http_data {
	int fd;
	enum conn_state state;

	/* { we may have a non-multithreaded HTTP discipline in the future */
		enum conn_action action;
		int pipe_fds[2];

		pthread_t io_thread;
		struct ringbuf *rb;

		struct condition full_cond;
		struct condition empty_cond;
		struct condition action_cond;
	/* } */

	int nr_redirect;
	size_t icy_metaint;
	size_t icy_offset;
	char *host;
	char *path;
	char *port;
	char *proxy_auth;
	char *http_auth;
};

static int awaken_buffer_task(struct http_data *data);

static void init_http_data(struct http_data *data)
{
	data->fd = -1;
	data->action = CONN_ACTION_NONE;
	data->state = CONN_STATE_NEW;
	init_async_pipe(data->pipe_fds);

	data->proxy_auth = proxy_host ?
	                   proxy_auth_string(proxy_user, proxy_password) :
	                   NULL;
	data->http_auth = NULL;
	data->host = NULL;
	data->path = NULL;
	data->port = NULL;
	data->nr_redirect = 0;
	data->icy_metaint = 0;
	data->icy_offset = 0;
	data->rb = ringbuf_create(buffer_size);

	cond_init(&data->action_cond);
	cond_init(&data->full_cond);
	cond_init(&data->empty_cond);
}

static struct http_data *new_http_data(void)
{
	struct http_data *ret = xmalloc(sizeof(struct http_data));
	init_http_data(ret);
	return ret;
}

static void free_http_data(struct http_data * data)
{
	if (data->host) free(data->host);
	if (data->path) free(data->path);
	if (data->port) free(data->port);
	if (data->proxy_auth) free(data->proxy_auth);
	if (data->http_auth) free(data->http_auth);

	cond_destroy(&data->action_cond);
	cond_destroy(&data->full_cond);
	cond_destroy(&data->empty_cond);

	xclose(data->pipe_fds[0]);
	xclose(data->pipe_fds[1]);
	ringbuf_free(data->rb);
	free(data);
}

static int parse_url(struct http_data * data, char *url)
{
	char *colon;
	char *slash;
	char *at;
	int len;
	char *cur = url;
	size_t offset;

	if (!match("http://"))
		return -1;

	cur = url + offset;
	colon = strchr(cur, ':');
	at = strchr(cur, '@');

	if (data->http_auth) {
		free(data->http_auth);
		data->http_auth = NULL;
	}

	if (at) {
		char *user;
		char *passwd;

		if (colon && colon < at) {
			user = xmalloc(colon - cur + 1);
			memcpy(user, cur, colon - cur);
			user[colon - cur] = '\0';

			passwd = xmalloc(at - colon);
			memcpy(passwd, colon + 1, at - colon - 1);
			passwd[at - colon - 1] = '\0';
		} else {
			user = xmalloc(at - cur + 1);
			memcpy(user, cur, at - cur);
			user[at - cur] = '\0';

			passwd = xstrdup("");
		}

		data->http_auth = http_auth_string(user, passwd);

		free(user);
		free(passwd);

		cur = at + 1;
		colon = strchr(cur, ':');
	}

	slash = strchr(cur, '/');

	if (slash && colon && slash <= colon)
		return -1;

	/* fetch the host portion */
	if (colon)
		len = colon - cur + 1;
	else if (slash)
		len = slash - cur + 1;
	else
		len = strlen(cur) + 1;

	if (len <= 1)
		return -1;

	if (data->host)
		free(data->host);
	data->host = xmalloc(len);
	memcpy(data->host, cur, len - 1);
	data->host[len - 1] = '\0';
	if (data->port)
		free(data->port);
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

	if (data->path)
		free(data->path);
	/* fetch the path */
	data->path = proxy_host ? xstrdup(url) : xstrdup(slash ? slash : "/");

	return 0;
}

/* triggers an action and waits for completion */
static int trigger_action(struct http_data *data,
                          enum conn_action action,
                          int nonblocking)
{
	int ret = -1;

	assert(!pthread_equal(data->io_thread, pthread_self()));
	cond_enter(&data->action_cond);
	if (data->action != CONN_ACTION_NONE)
		goto out;
	data->action = action;
	if (awaken_buffer_task(data)) {
		/* DEBUG("wokeup from cond_wait to trigger action\n"); */
	} else if (xwrite(data->pipe_fds[1], "", 1) != 1) {
		ERROR(__FILE__ ": pipe full, couldn't trigger action\n");
		data->action = CONN_ACTION_NONE;
		goto out;
	}
	if (nonblocking)
		cond_timedwait(&data->action_cond, 1);
	else
		cond_wait(&data->action_cond);
	ret = 0;
out:
	cond_leave(&data->action_cond);
	return ret;
}

static int take_action(struct http_data *data)
{
	assert(pthread_equal(data->io_thread, pthread_self()));

	cond_enter(&data->action_cond);
	switch (data->action) {
	case CONN_ACTION_NONE:
		cond_leave(&data->action_cond);
		return 0;
	case CONN_ACTION_DOSEEK:
		data->state = CONN_STATE_NEW;
		break;
	case CONN_ACTION_CLOSE:
		data->state = CONN_STATE_CLOSED;
	}
	xclose(data->fd);
	data->fd = -1;
	data->action = CONN_ACTION_NONE;
	cond_signal_sync(&data->action_cond);
	cond_leave(&data->action_cond);
	return 1;
}

static int err_close(struct http_data *data)
{
	assert(pthread_equal(data->io_thread, pthread_self()));
	xclose(data->fd);
	data->state = CONN_STATE_CLOSED;
	return -1;
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

static int init_connection(struct http_data *data)
{
	struct addrinfo *ans = NULL;

	assert(pthread_equal(data->io_thread, pthread_self()));
	assert_state2(CONN_STATE_NEW, CONN_STATE_REDIRECT);

	if ((proxy_host ? my_getaddrinfo(&ans, proxy_host, proxy_port) :
	                  my_getaddrinfo(&ans, data->host, data->port)) < 0)
		return -1;

	assert(data->fd < 0);
	data->fd = my_connect_addrs(ans);
	freeaddrinfo(ans);

	if (data->fd < 0)
		return -1; /* failed */
	data->state = CONN_STATE_CONNECTED;
	return 0;
}

#define my_nfds(d) ((d->fd > d->pipe_fds[0] ? d->fd : d->pipe_fds[0]) + 1)

static int pipe_notified(struct http_data * data, fd_set *rfds)
{
	char buf;
	int fd = data->pipe_fds[0];

	assert(pthread_equal(data->io_thread, pthread_self()));
	return FD_ISSET(fd, rfds) && (xread(fd, &buf, 1) == 1);
}

enum await_result {
	AWAIT_READY,
	AWAIT_ACTION_PENDING,
	AWAIT_ERROR
};

static enum await_result socket_error_or_ready(int fd)
{
	int ret;
	int error = 0;
	socklen_t error_len = sizeof(int);

	ret = getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &error_len);
	return (ret < 0 || error) ? AWAIT_ERROR : AWAIT_READY;
}

static enum await_result await_sendable(struct http_data *data)
{
	fd_set rfds, wfds;

	assert(pthread_equal(data->io_thread, pthread_self()));
	assert_state(CONN_STATE_CONNECTED);

	FD_ZERO(&rfds);
	FD_ZERO(&wfds);
	FD_SET(data->pipe_fds[0], &rfds);
	FD_SET(data->fd, &wfds);

	if (select(my_nfds(data), &rfds, &wfds, NULL, NULL) <= 0)
		return AWAIT_ERROR;
	if (pipe_notified(data, &rfds)) return AWAIT_ACTION_PENDING;
	return socket_error_or_ready(data->fd);
}

static enum await_result await_recvable(struct http_data *data)
{
	fd_set rfds;

	assert(pthread_equal(data->io_thread, pthread_self()));

	FD_ZERO(&rfds);
	FD_SET(data->pipe_fds[0], &rfds);
	FD_SET(data->fd, &rfds);

	if (select(my_nfds(data), &rfds, NULL, NULL, NULL) <= 0)
		return AWAIT_ERROR;
	if (pipe_notified(data, &rfds)) return AWAIT_ACTION_PENDING;
	return socket_error_or_ready(data->fd);
}

static void await_buffer_space(struct http_data *data)
{
	assert(pthread_equal(data->io_thread, pthread_self()));
	assert_state(CONN_STATE_BUFFER_FULL);
	cond_wait(&data->full_cond);
	if (ringbuf_write_space(data->rb) > 0)
		data->state = CONN_STATE_BUFFER;
	/* else spurious wakeup or action triggered ... */
}

static void feed_starved(struct http_data *data)
{
	assert(pthread_equal(data->io_thread, pthread_self()));
	cond_signal_async(&data->empty_cond);
}

static int starved_wait(struct http_data *data, const long sec)
{
	assert(!pthread_equal(data->io_thread, pthread_self()));
	return cond_timedwait(&data->empty_cond, sec);
}

static int awaken_buffer_task(struct http_data *data)
{
	assert(!pthread_equal(data->io_thread, pthread_self()));

	return ! cond_signal_async(&data->full_cond);
}

static ssize_t buffer_data(InputStream *is)
{
	struct iovec vec[2];
	ssize_t r;
	struct http_data *data = (struct http_data *)is->data;

	assert(pthread_equal(data->io_thread, pthread_self()));
	assert_state2(CONN_STATE_BUFFER, CONN_STATE_PREBUFFER);

	if (!ringbuf_get_write_vector(data->rb, vec)) {
		data->state = CONN_STATE_BUFFER_FULL;
		return 0;
	}
	r = readv(data->fd, vec, vec[1].iov_len ? 2 : 1);
	if (r > 0) {
		size_t buflen;

		ringbuf_write_advance(data->rb, r);
		buflen = ringbuf_read_space(data->rb);
		if (buflen == 0 || buflen < data->icy_metaint)
			data->state = CONN_STATE_PREBUFFER;
		else if (buflen >= prebuffer_size)
			data->state = CONN_STATE_BUFFER;
		if (data->state == CONN_STATE_BUFFER)
			feed_starved(data);
		return r;
	} else if (r < 0) {
		if (errno == EAGAIN || errno == EINTR)
			return 0;
		is->error = errno;
	}
	err_close(data);
	return r;
}

/*
 * This requires the socket to be writable beforehand (determined via
 * select(2)).  This does NOT retry or continue if we can't write the
 * HTTP header in one shot.  One reason for this is laziness, I don't
 * want to have to store the header when recalling this function, but
 * the other reason is practical, too: if we can't send a small HTTP
 * request without blocking, the connection is pathetic anyways and we
 * should just stop
 *
 * Returns -1 on error, 0 on success
 */
static int send_request(InputStream * is)
{
	struct http_data *data = (struct http_data *) is->data;
	int length;
	ssize_t nbytes;
	char request[2048]; /* todo(?): write item-at-a-time and cork */

	assert(pthread_equal(data->io_thread, pthread_self()));
	assert_state(CONN_STATE_CONNECTED);
	length = snprintf(request, sizeof(request),
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
	                 is->offset,
	                 data->proxy_auth ? data->proxy_auth :
	                  (data->http_auth ? data->http_auth : ""));
	if (length < 0 || length >= (int)sizeof(request))
		return err_close(data);
	nbytes = write(data->fd, request, (size_t)length);
	if (nbytes < 0 || nbytes != (ssize_t)length)
		return err_close(data);
	data->state = CONN_STATE_REQUESTED;
	return 0;
}

/* handles parsing of the first line of the HTTP response */
static int parse_response_code(InputStream * is, const char *response)
{
	size_t offset;
	const char *cur = response;

	is->seekable = 0;
	if (match("HTTP/1.0 ")) {
		return atoi(cur + offset);
	} else if (match("HTTP/1.1 ")) {
		is->seekable = 1;
		return atoi(cur + offset);
	} else if (match("ICY 200 OK")) {
		return 200;
	} else if (match("ICY 400 Server Full")) {
		return 400;
	} else if (match("ICY 404"))
		return 404;
	return 0;
}

static int leading_space(int c)
{
	return (c == ' ' || c == '\t');
}

static int parse_header_dup(char **dst, char *cur)
{
	char *eol;
	size_t len;

	if (!(eol = strstr(cur, "\r\n")))
		return -1;
	*eol = '\0';
	while (leading_space(*cur))
		cur++;
	len = strlen(cur) + 1;
	*dst = xrealloc(*dst, len);
	memcpy(*dst, cur, len);
	*eol = '\r';
	return 0;
}

static int parse_redirect(InputStream * is, char *response, const char *needle)
{
	char *url = NULL;
	char *cur = strstr(response, "\r\n");
	size_t offset;
	struct http_data *data = (struct http_data *) is->data;
	int ret;

	while (cur && cur != needle) {
		assert(cur < needle);
		if (match("\r\nLocation:"))
			goto found;
		cur = strstr(cur + 2, "\r\n");
	}
	return -1;
found:
	if (parse_header_dup(&url, cur + offset) < 0)
		return -1;
	ret = parse_url(data, url);
	free(url);
	if (!ret && data->nr_redirect < HTTP_REDIRECT_MAX) {
		data->nr_redirect++;
		xclose(data->fd);
		data->fd = -1;
		data->state = CONN_STATE_REDIRECT;
		is->ready = 1;
		return 0; /* success */
	}
	return -1;
}

static int parse_headers(InputStream * is, char *response, const char *needle)
{
	struct http_data *data = (struct http_data *) is->data;
	char *cur = strstr(response, "\r\n");
	size_t offset;
	long tmp;

	data->icy_metaint = 0;
	data->icy_offset = 0;
	if (is->mime) {
		free(is->mime);
		is->mime = NULL;
	}
	if (is->metaName) {
		free(is->metaName);
		is->metaName = NULL;
	}
	is->size = 0;

	while (cur && cur != needle) {
		assert(cur < needle);
		if (match("\r\nContent-Length:")) {
			if ((tmp = atol(cur + offset)) >= 0)
				is->size = tmp;
		} else if (match("\r\nicy-metaint:")) {
			if ((tmp = atol(cur + offset)) >= 0)
				data->icy_metaint = tmp;
		} else if (match("\r\nicy-name:") ||
		           match("\r\nice-name:") ||
		           match("\r\nx-audiocast-name:")) {
			if (parse_header_dup(&is->metaName, cur + offset) < 0)
				return -1;
			DEBUG(__FILE__": metaName: %s\n", is->metaName);
		} else if (match("\r\nContent-Type:")) {
			if (parse_header_dup(&is->mime, cur + offset) < 0)
				return -1;
		}
		cur = strstr(cur + 2, "\r\n");
	}
	return 0;
}

/* Returns -1 on error, 0 on success */
static int recv_response(InputStream * is)
{
	struct http_data *data = (struct http_data *) is->data;
	char *needle;
	char response[2048];
	const size_t response_max = sizeof(response) - 1;
	ssize_t r;
	ssize_t peeked;

	assert(pthread_equal(data->io_thread, pthread_self()));
	assert_state2(CONN_STATE_RESP_HEAD, CONN_STATE_REQUESTED);
	do {
		r = recv(data->fd, response, response_max, MSG_PEEK);
	} while (r < 0 && errno == EINTR);
	if (r <= 0)
		return err_close(data); /* EOF */
	response[r] = '\0';
	if (!(needle = strstr(response, "\r\n\r\n"))) {
		if ((size_t)r == response_max)
			return err_close(data);
		/* response too small, try again */
		data->state = CONN_STATE_RESP_HEAD;
		return -1;
	}

	switch (parse_response_code(is, response)) {
	case 200: /* OK */
	case 206: /* Partial Content */
		break;
	case 301: /* Moved Permanently */
	case 302: /* Moved Temporarily */
		if (parse_redirect(is, response, needle) == 0)
			return 0; /* success, reconnect */
	default:
		return err_close(data);
	}

	parse_headers(is, response, needle);
	if (is->size <= 0)
		is->seekable = 0;
	needle += sizeof("\r\n\r\n") - 1;
	peeked = needle - response;
	assert(peeked <= r);
	do {
		r = recv(data->fd, response, peeked, 0);
	} while (r < 0 && errno == EINTR);
	assert(r == peeked && "r != peeked");

	ringbuf_writer_reset(data->rb);
	data->state = CONN_STATE_PREBUFFER;
	is->ready = 1;

	return 0;
}

static void * http_io_task(void *arg)
{
	InputStream *is = (InputStream *) arg;
	struct http_data *data = (struct http_data *) is->data;

	cond_enter(&data->full_cond);
	while (1) {
		take_action(data);
		switch (data->state) {
		case CONN_STATE_NEW:
		case CONN_STATE_REDIRECT:
			init_connection(data);
			break;
		case CONN_STATE_CONNECTED:
			switch (await_sendable(data)) {
			case AWAIT_READY: send_request(is); break;
			case AWAIT_ACTION_PENDING: break;
			case AWAIT_ERROR: goto err;
			}
			break;
		case CONN_STATE_REQUESTED:
		case CONN_STATE_RESP_HEAD:
			switch (await_recvable(data)) {
			case AWAIT_READY: recv_response(is); break;
			case AWAIT_ACTION_PENDING: break;
			case AWAIT_ERROR: goto err;
			}
			break;
		case CONN_STATE_PREBUFFER:
		case CONN_STATE_BUFFER:
			switch (await_recvable(data)) {
			case AWAIT_READY: buffer_data(is); break;
			case AWAIT_ACTION_PENDING: break;
			case AWAIT_ERROR: goto err;
			}
			break;
		case CONN_STATE_BUFFER_FULL:
			await_buffer_space(data);
			break;
		case CONN_STATE_CLOSED: goto closed;
		}
	}
err:
	err_close(data);
closed:
	assert_state(CONN_STATE_CLOSED);
	cond_leave(&data->full_cond);
	return NULL;
}

int inputStream_httpBuffer(mpd_unused InputStream *is)
{
	return 0;
}

int inputStream_httpOpen(InputStream * is, char *url)
{
	struct http_data *data = new_http_data();
	pthread_attr_t attr;

	is->seekable = 0;
	is->data = data;
	if (parse_url(data, url) < 0) {
		free_http_data(data);
		return -1;
	}

	is->seekFunc = inputStream_httpSeek;
	is->closeFunc = inputStream_httpClose;
	is->readFunc = inputStream_httpRead;
	is->atEOFFunc = inputStream_httpAtEOF;
	is->bufferFunc = inputStream_httpBuffer;

	pthread_attr_init(&attr);
	if (pthread_create(&data->io_thread, &attr, http_io_task, is))
		FATAL("failed to spawn http_io_task: %s", strerror(errno));

	cond_enter(&data->empty_cond); /* httpClose will leave this */
	return 0;
}

int inputStream_httpSeek(InputStream * is, long offset, int whence)
{
	struct http_data *data = (struct http_data *)is->data;
	long old_offset = is->offset;
	long diff;

	if (!is->seekable) {
		is->error = ESPIPE;
		return -1;
	}
	assert(is->size > 0);

	switch (whence) {
	case SEEK_SET:
		is->offset = offset;
		break;
	case SEEK_CUR:
		is->offset += offset;
		break;
	case SEEK_END:
		is->offset = is->size + offset;
		break;
	default:
		is->error = EINVAL;
		return -1;
	}

	diff = is->offset - old_offset;
	if (!diff)
		return 0; /* nothing to seek */
	if (diff > 0) { /* seek forward if we've already buffered it */
		long avail = (long)ringbuf_read_space(data->rb);
		if (avail >= diff) {
			ringbuf_read_advance(data->rb, diff);
			return 0;
		}
	}
	trigger_action(data, CONN_ACTION_DOSEEK, 0);
	return 0;
}

static void parse_icy_metadata(InputStream * is, char *metadata, size_t size)
{
	char *r = NULL;
	char *cur;
	size_t offset;

	assert(size);
	metadata[size] = '\0';
	cur = strtok_r(metadata, ";", &r);
	while (cur) {
		if (match("StreamTitle=")) {
			if (is->metaTitle)
				free(is->metaTitle);
			if (cur[offset] == '\'')
				offset++;
			if (r[-2] == '\'')
				r[-2] = '\0';
			is->metaTitle = xstrdup(cur + offset);
			DEBUG(__FILE__ ": metaTitle: %s\n", is->metaTitle);
			return;
		}
		cur = strtok_r(NULL, ";", &r);
	}
}

static size_t read_with_metadata(InputStream *is, unsigned char *ptr,
				 ssize_t len)
{
	struct http_data *data = (struct http_data *) is->data;
	size_t readed = 0;
	size_t r;
	size_t to_read;
	assert(data->icy_metaint > 0);

	while (len > 0) {
		if (ringbuf_read_space(data->rb) < data->icy_metaint)
			break;
		if (data->icy_offset >= data->icy_metaint) {
			unsigned char metabuf[(UCHAR_MAX << 4) + 1];
			size_t metalen;
			r = ringbuf_read(data->rb, metabuf, 1);
			assert(r == 1 && "failed to read");
			awaken_buffer_task(data);
			metalen = *(metabuf);
			metalen <<= 4;
			if (metalen) {
				r = ringbuf_read(data->rb, metabuf, metalen);
				assert(r == metalen && "short metadata read");
				parse_icy_metadata(is, (char*)metabuf, metalen);
			}
			data->icy_offset = 0;
		}
		to_read = len;
		if (to_read > (data->icy_metaint - data->icy_offset))
			to_read = data->icy_metaint - data->icy_offset;
		if (!(r = ringbuf_read(data->rb, ptr, to_read)))
			break;
		awaken_buffer_task(data);
		len -= r;
		ptr += r;
		readed += r;
		data->icy_offset += r;
	}
	return readed;
}

size_t inputStream_httpRead(InputStream * is, void *_ptr, size_t size)
{
	struct http_data *data = (struct http_data *) is->data;
	size_t len = size;
	size_t r;
	unsigned char *ptr = _ptr, *ptr0 = _ptr;
	long tries = len / 128; /* try harder for bigger reads */

retry:
	switch (data->state) {
	case CONN_STATE_NEW:
	case CONN_STATE_REDIRECT:
	case CONN_STATE_CONNECTED:
	case CONN_STATE_REQUESTED:
	case CONN_STATE_RESP_HEAD:
	case CONN_STATE_PREBUFFER:
		if ((starved_wait(data, 1) == 0) || (tries-- > 0))
			goto retry; /* success */
		return 0;
	case CONN_STATE_BUFFER:
	case CONN_STATE_BUFFER_FULL:
		break;
	case CONN_STATE_CLOSED:
		if (!ringbuf_read_space(data->rb))
			return 0;
	}

	while (1) {
		if (data->icy_metaint > 0)
			r = read_with_metadata(is, ptr, len);
		else /* easy, no metadata to worry about */
			r = ringbuf_read(data->rb, ptr, len);
		assert(r <= len);
		if (r) {
			awaken_buffer_task(data);
			is->offset += r;
			ptr += r;
			len -= r;
		}
		if (!len || (--tries < 0) ||
		    (data->state == CONN_STATE_CLOSED &&
		     !ringbuf_read_space(data->rb)))
			break;
		starved_wait(data, 1);
	}
	return (ptr - ptr0) / size;
}

int inputStream_httpClose(InputStream * is)
{
	struct http_data *data = (struct http_data *) is->data;

	/*
	 * The cancellation routines in pthreads suck (and
	 * are probably unportable) and using signal handlers
	 * between threads is _definitely_ unportable.
	 */
	while (data->state != CONN_STATE_CLOSED)
		trigger_action(data, CONN_ACTION_CLOSE, 1);
	pthread_join(data->io_thread, NULL);
	cond_leave(&data->empty_cond);
	free_http_data(data);
	return 0;
}

int inputStream_httpAtEOF(InputStream * is)
{
	struct http_data *data = (struct http_data *) is->data;
	if (data->state == CONN_STATE_CLOSED && !ringbuf_read_space(data->rb))
		return 1;
	return 0;
}

void inputStream_initHttp(void)
{
	ConfigParam *param = getConfigParam(CONF_HTTP_PROXY_HOST);
	char *test;
	if (param) {
		proxy_host = param->value;

		param = getConfigParam(CONF_HTTP_PROXY_PORT);

		if (!param) {
			FATAL("%s specified but not %s\n", CONF_HTTP_PROXY_HOST,
			      CONF_HTTP_PROXY_PORT);
		}
		proxy_port = param->value;

		param = getConfigParam(CONF_HTTP_PROXY_USER);

		if (param) {
			proxy_user = param->value;

			param = getConfigParam(CONF_HTTP_PROXY_PASSWORD);

			if (!param) {
				FATAL("%s specified but not %s\n",
				      CONF_HTTP_PROXY_USER,
				      CONF_HTTP_PROXY_PASSWORD);
			}

			proxy_password = param->value;
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

		buffer_size = tmp * 1024;
	}
	if (buffer_size < 4096)
		FATAL(CONF_HTTP_BUFFER_SIZE" must be >= 4KB\n");

	param = getConfigParam(CONF_HTTP_PREBUFFER_SIZE);

	if (param) {
		long tmp = strtol(param->value, &test, 10);
		if (*test != '\0' || tmp <= 0) {
			FATAL("\"%s\" specified for %s at line %i is not a "
			      "positive integer\n",
			      param->value, CONF_HTTP_PREBUFFER_SIZE,
			      param->line);
		}

		prebuffer_size = tmp * 1024;
	}

	if (prebuffer_size > buffer_size)
		prebuffer_size = buffer_size;
	assert(buffer_size > 0 && "http buffer_size too small");
	assert(prebuffer_size > 0 && "http prebuffer_size too small");
}

