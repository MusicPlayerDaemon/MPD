/*
 * Copyright (C) 2003-2013 The Music Player Daemon Project
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

#include "config.h"
#include "HttpdOutputPlugin.hxx"
#include "HttpdInternal.hxx"
#include "HttpdClient.hxx"
#include "output_api.h"
#include "encoder_plugin.h"
#include "encoder_list.h"
#include "resolver.h"
#include "page.h"
#include "icy_server.h"
#include "fd_util.h"
#include "ServerSocket.hxx"

#include <assert.h>

#include <sys/types.h>
#include <unistd.h>
#include <errno.h>

#ifdef HAVE_LIBWRAP
#include <sys/socket.h> /* needed for AF_UNIX */
#include <tcpd.h>
#endif

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "httpd_output"

/**
 * The quark used for GError.domain.
 */
static inline GQuark
httpd_output_quark(void)
{
	return g_quark_from_static_string("httpd_output");
}

/**
 * Check whether there is at least one client.
 *
 * Caller must lock the mutex.
 */
G_GNUC_PURE
static bool
httpd_output_has_clients(const httpd_output *httpd)
{
	return !httpd->clients.empty();
}

/**
 * Check whether there is at least one client.
 */
G_GNUC_PURE
static bool
httpd_output_lock_has_clients(const httpd_output *httpd)
{
	const ScopeLock protect(httpd->mutex);
	return httpd_output_has_clients(httpd);
}

static void
httpd_listen_in_event(int fd, const struct sockaddr *address,
		      size_t address_length, int uid, void *ctx);

static bool
httpd_output_bind(httpd_output *httpd, GError **error_r)
{
	httpd->open = false;

	const ScopeLock protect(httpd->mutex);
	return server_socket_open(httpd->server_socket, error_r);
}

static void
httpd_output_unbind(httpd_output *httpd)
{
	assert(!httpd->open);

	const ScopeLock protect(httpd->mutex);
	server_socket_close(httpd->server_socket);
}

static struct audio_output *
httpd_output_init(const struct config_param *param,
		  GError **error)
{
	httpd_output *httpd = new httpd_output();
	if (!ao_base_init(&httpd->base, &httpd_output_plugin, param, error)) {
		g_free(httpd);
		return NULL;
	}

	/* read configuration */
	httpd->name =
		config_get_block_string(param, "name", "Set name in config");
	httpd->genre =
		config_get_block_string(param, "genre", "Set genre in config");
	httpd->website =
		config_get_block_string(param, "website", "Set website in config");

	guint port = config_get_block_unsigned(param, "port", 8000);

	const char *encoder_name =
		config_get_block_string(param, "encoder", "vorbis");
	const struct encoder_plugin *encoder_plugin =
		encoder_plugin_get(encoder_name);
	if (encoder_plugin == NULL) {
		g_set_error(error, httpd_output_quark(), 0,
			    "No such encoder: %s", encoder_name);
		ao_base_finish(&httpd->base);
		g_free(httpd);
		return NULL;
	}

	httpd->clients_max = config_get_block_unsigned(param,"max_clients", 0);

	/* set up bind_to_address */

	httpd->server_socket = server_socket_new(httpd_listen_in_event, httpd);

	const char *bind_to_address =
		config_get_block_string(param, "bind_to_address", NULL);
	bool success = bind_to_address != NULL &&
		strcmp(bind_to_address, "any") != 0
		? server_socket_add_host(httpd->server_socket, bind_to_address,
					 port, error)
		: server_socket_add_port(httpd->server_socket, port, error);
	if (!success) {
		ao_base_finish(&httpd->base);
		g_free(httpd);
		return NULL;
	}

	/* initialize metadata */
	httpd->metadata = NULL;
	httpd->unflushed_input = 0;

	/* initialize encoder */

	httpd->encoder = encoder_init(encoder_plugin, param, error);
	if (httpd->encoder == NULL) {
		ao_base_finish(&httpd->base);
		g_free(httpd);
		return NULL;
	}

	/* determine content type */
	httpd->content_type = encoder_get_mime_type(httpd->encoder);
	if (httpd->content_type == NULL) {
		httpd->content_type = "application/octet-stream";
	}

	return &httpd->base;
}

static void
httpd_output_finish(struct audio_output *ao)
{
	httpd_output *httpd = (httpd_output *)ao;

	if (httpd->metadata)
		page_unref(httpd->metadata);

	encoder_finish(httpd->encoder);
	server_socket_free(httpd->server_socket);
	ao_base_finish(&httpd->base);
	delete httpd;
}

/**
 * Creates a new #HttpdClient object and adds it into the
 * httpd_output.clients linked list.
 */
static void
httpd_client_add(httpd_output *httpd, int fd)
{
	httpd->clients.emplace_front(httpd, fd,
				     httpd->encoder->plugin->tag == NULL);
	httpd->clients_cnt++;

	/* pass metadata to client */
	if (httpd->metadata)
		httpd->clients.front().PushMetaData(httpd->metadata);
}

static void
httpd_listen_in_event(int fd, const struct sockaddr *address,
		      size_t address_length, G_GNUC_UNUSED int uid, void *ctx)
{
	httpd_output *httpd = (httpd_output *)ctx;

	/* the listener socket has become readable - a client has
	   connected */

#ifdef HAVE_LIBWRAP
	if (address->sa_family != AF_UNIX) {
		char *hostaddr = sockaddr_to_string(address, address_length, NULL);
		const char *progname = g_get_prgname();

		struct request_info req;
		request_init(&req, RQ_FILE, fd, RQ_DAEMON, progname, 0);

		fromhost(&req);

		if (!hosts_access(&req)) {
			/* tcp wrappers says no */
			g_warning("libwrap refused connection (libwrap=%s) from %s",
			      progname, hostaddr);
			g_free(hostaddr);
			close_socket(fd);
			return;
		}

		g_free(hostaddr);
	}
#else
	(void)address;
	(void)address_length;
#endif	/* HAVE_WRAP */

	const ScopeLock protect(httpd->mutex);

	if (fd >= 0) {
		/* can we allow additional client */
		if (httpd->open &&
		    (httpd->clients_max == 0 ||
		     httpd->clients_cnt < httpd->clients_max))
			httpd_client_add(httpd, fd);
		else
			close_socket(fd);
	} else if (fd < 0 && errno != EINTR) {
		g_warning("accept() failed: %s", g_strerror(errno));
	}
}

/**
 * Reads data from the encoder (as much as available) and returns it
 * as a new #page object.
 */
static struct page *
httpd_output_read_page(httpd_output *httpd)
{
	if (httpd->unflushed_input >= 65536) {
		/* we have fed a lot of input into the encoder, but it
		   didn't give anything back yet - flush now to avoid
		   buffer underruns */
		encoder_flush(httpd->encoder, NULL);
		httpd->unflushed_input = 0;
	}

	size_t size = 0;
	do {
		size_t nbytes = encoder_read(httpd->encoder,
					     httpd->buffer + size,
					     sizeof(httpd->buffer) - size);
		if (nbytes == 0)
			break;

		httpd->unflushed_input = 0;

		size += nbytes;
	} while (size < sizeof(httpd->buffer));

	if (size == 0)
		return NULL;

	return page_new_copy(httpd->buffer, size);
}

static bool
httpd_output_encoder_open(httpd_output *httpd,
			  struct audio_format *audio_format,
			  GError **error)
{
	if (!encoder_open(httpd->encoder, audio_format, error))
		return false;

	/* we have to remember the encoder header, i.e. the first
	   bytes of encoder output after opening it, because it has to
	   be sent to every new client */
	httpd->header = httpd_output_read_page(httpd);

	httpd->unflushed_input = 0;

	return true;
}

static bool
httpd_output_enable(struct audio_output *ao, GError **error_r)
{
	httpd_output *httpd = (httpd_output *)ao;

	return httpd_output_bind(httpd, error_r);
}

static void
httpd_output_disable(struct audio_output *ao)
{
	httpd_output *httpd = (httpd_output *)ao;

	httpd_output_unbind(httpd);
}

static bool
httpd_output_open(struct audio_output *ao, struct audio_format *audio_format,
		  GError **error)
{
	httpd_output *httpd = (httpd_output *)ao;

	assert(httpd->clients.empty());

	const ScopeLock protect(httpd->mutex);

	/* open the encoder */

	if (!httpd_output_encoder_open(httpd, audio_format, error))
		return false;

	/* initialize other attributes */

	httpd->clients_cnt = 0;
	httpd->timer = timer_new(audio_format);

	httpd->open = true;

	return true;
}

static void
httpd_output_close(struct audio_output *ao)
{
	httpd_output *httpd = (httpd_output *)ao;

	const ScopeLock protect(httpd->mutex);

	httpd->open = false;

	timer_free(httpd->timer);

	httpd->clients.clear();

	if (httpd->header != NULL)
		page_unref(httpd->header);

	encoder_close(httpd->encoder);
}

void
httpd_output_remove_client(httpd_output *httpd, HttpdClient *client)
{
	assert(httpd != NULL);
	assert(httpd->clients_cnt > 0);
	assert(client != NULL);

	for (auto prev = httpd->clients.before_begin(), i = std::next(prev);;
	     prev = i, i = std::next(prev)) {
		assert(i != httpd->clients.end());
		if (&*i == client) {
			httpd->clients.erase_after(prev);
			httpd->clients_cnt--;
			break;
		}
	}
}

void
httpd_output_send_header(httpd_output *httpd, HttpdClient *client)
{
	if (httpd->header != NULL)
		client->PushPage(httpd->header);
}

static unsigned
httpd_output_delay(struct audio_output *ao)
{
	httpd_output *httpd = (httpd_output *)ao;

	if (!httpd_output_lock_has_clients(httpd) && httpd->base.pause) {
		/* if there's no client and this output is paused,
		   then httpd_output_pause() will not do anything, it
		   will not fill the buffer and it will not update the
		   timer; therefore, we reset the timer here */
		timer_reset(httpd->timer);

		/* some arbitrary delay that is long enough to avoid
		   consuming too much CPU, and short enough to notice
		   new clients quickly enough */
		return 1000;
	}

	return httpd->timer->started
		? timer_delay(httpd->timer)
		: 0;
}

/**
 * Broadcasts a page struct to all clients.
 */
static void
httpd_output_broadcast_page(httpd_output *httpd, struct page *page)
{
	assert(page != NULL);

	const ScopeLock protect(httpd->mutex);
	for (auto &client : httpd->clients)
		client.PushPage(page);
}

/**
 * Broadcasts data from the encoder to all clients.
 */
static void
httpd_output_encoder_to_clients(httpd_output *httpd)
{
	httpd->mutex.lock();
	for (auto &client : httpd->clients) {
		if (client.GetQueueSize() > 256 * 1024) {
			g_debug("client is too slow, flushing its queue");
			client.CancelQueue();
		}
	}
	httpd->mutex.unlock();

	struct page *page;
	while ((page = httpd_output_read_page(httpd)) != NULL) {
		httpd_output_broadcast_page(httpd, page);
		page_unref(page);
	}
}

static bool
httpd_output_encode_and_play(httpd_output *httpd,
			     const void *chunk, size_t size, GError **error)
{
	if (!encoder_write(httpd->encoder, chunk, size, error))
		return false;

	httpd->unflushed_input += size;

	httpd_output_encoder_to_clients(httpd);

	return true;
}

static size_t
httpd_output_play(struct audio_output *ao, const void *chunk, size_t size,
		  GError **error_r)
{
	httpd_output *httpd = (httpd_output *)ao;

	if (httpd_output_lock_has_clients(httpd)) {
		if (!httpd_output_encode_and_play(httpd, chunk, size, error_r))
			return 0;
	}

	if (!httpd->timer->started)
		timer_start(httpd->timer);
	timer_add(httpd->timer, size);

	return size;
}

static bool
httpd_output_pause(struct audio_output *ao)
{
	httpd_output *httpd = (httpd_output *)ao;

	if (httpd_output_lock_has_clients(httpd)) {
		static const char silence[1020] = { 0 };
		return httpd_output_play(ao, silence, sizeof(silence),
					 NULL) > 0;
	} else {
		return true;
	}
}

static void
httpd_output_tag(struct audio_output *ao, const struct tag *tag)
{
	httpd_output *httpd = (httpd_output *)ao;

	assert(tag != NULL);

	if (httpd->encoder->plugin->tag != NULL) {
		/* embed encoder tags */

		/* flush the current stream, and end it */

		encoder_pre_tag(httpd->encoder, NULL);
		httpd_output_encoder_to_clients(httpd);

		/* send the tag to the encoder - which starts a new
		   stream now */

		encoder_tag(httpd->encoder, tag, NULL);

		/* the first page generated by the encoder will now be
		   used as the new "header" page, which is sent to all
		   new clients */

		struct page *page = httpd_output_read_page(httpd);
		if (page != NULL) {
			if (httpd->header != NULL)
				page_unref(httpd->header);
			httpd->header = page;
			httpd_output_broadcast_page(httpd, page);
		}
	} else {
		/* use Icy-Metadata */

		if (httpd->metadata != NULL)
			page_unref (httpd->metadata);

		httpd->metadata =
			icy_server_metadata_page(tag, TAG_ALBUM,
						 TAG_ARTIST, TAG_TITLE,
						 TAG_NUM_OF_ITEM_TYPES);
		if (httpd->metadata != NULL) {
			const ScopeLock protect(httpd->mutex);
			for (auto &client : httpd->clients)
				client.PushMetaData(httpd->metadata);
		}
	}
}

static void
httpd_output_cancel(struct audio_output *ao)
{
	httpd_output *httpd = (httpd_output *)ao;

	const ScopeLock protect(httpd->mutex);
	for (auto &client : httpd->clients)
		client.CancelQueue();
}

const struct audio_output_plugin httpd_output_plugin = {
	"httpd",
	nullptr,
	httpd_output_init,
	httpd_output_finish,
	httpd_output_enable,
	httpd_output_disable,
	httpd_output_open,
	httpd_output_close,
	httpd_output_delay,
	httpd_output_tag,
	httpd_output_play,
	nullptr,
	httpd_output_cancel,
	httpd_output_pause,
	nullptr,
};
