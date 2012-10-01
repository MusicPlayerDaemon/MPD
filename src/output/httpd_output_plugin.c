/*
 * Copyright (C) 2003-2011 The Music Player Daemon Project
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
#include "httpd_output_plugin.h"
#include "httpd_internal.h"
#include "httpd_client.h"
#include "output_api.h"
#include "encoder_plugin.h"
#include "encoder_list.h"
#include "resolver.h"
#include "page.h"
#include "icy_server.h"
#include "fd_util.h"
#include "server_socket.h"

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
httpd_output_has_clients(const struct httpd_output *httpd)
{
	return httpd->clients != NULL;
}

/**
 * Check whether there is at least one client.
 */
G_GNUC_PURE
static bool
httpd_output_lock_has_clients(const struct httpd_output *httpd)
{
	g_mutex_lock(httpd->mutex);
	bool result = httpd_output_has_clients(httpd);
	g_mutex_unlock(httpd->mutex);
	return result;
}

static void
httpd_listen_in_event(int fd, const struct sockaddr *address,
		      size_t address_length, int uid, void *ctx);

static bool
httpd_output_bind(struct httpd_output *httpd, GError **error_r)
{
	httpd->open = false;

	g_mutex_lock(httpd->mutex);
	bool success = server_socket_open(httpd->server_socket, error_r);
	g_mutex_unlock(httpd->mutex);

	return success;
}

static void
httpd_output_unbind(struct httpd_output *httpd)
{
	assert(!httpd->open);

	g_mutex_lock(httpd->mutex);
	server_socket_close(httpd->server_socket);
	g_mutex_unlock(httpd->mutex);
}

static struct audio_output *
httpd_output_init(const struct config_param *param,
		  GError **error)
{
	struct httpd_output *httpd = g_new(struct httpd_output, 1);
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

	httpd->mutex = g_mutex_new();

	return &httpd->base;
}

static void
httpd_output_finish(struct audio_output *ao)
{
	struct httpd_output *httpd = (struct httpd_output *)ao;

	if (httpd->metadata)
		page_unref(httpd->metadata);

	encoder_finish(httpd->encoder);
	server_socket_free(httpd->server_socket);
	g_mutex_free(httpd->mutex);
	ao_base_finish(&httpd->base);
	g_free(httpd);
}

/**
 * Creates a new #httpd_client object and adds it into the
 * httpd_output.clients linked list.
 */
static void
httpd_client_add(struct httpd_output *httpd, int fd)
{
	struct httpd_client *client =
		httpd_client_new(httpd, fd,
				 httpd->encoder->plugin->tag == NULL);

	httpd->clients = g_list_prepend(httpd->clients, client);
	httpd->clients_cnt++;

	/* pass metadata to client */
	if (httpd->metadata)
		httpd_client_send_metadata(client, httpd->metadata);
}

static void
httpd_listen_in_event(int fd, const struct sockaddr *address,
		      size_t address_length, G_GNUC_UNUSED int uid, void *ctx)
{
	struct httpd_output *httpd = ctx;

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
			g_mutex_unlock(httpd->mutex);
			return;
		}

		g_free(hostaddr);
	}
#else
	(void)address;
	(void)address_length;
#endif	/* HAVE_WRAP */

	g_mutex_lock(httpd->mutex);

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

	g_mutex_unlock(httpd->mutex);
}

/**
 * Reads data from the encoder (as much as available) and returns it
 * as a new #page object.
 */
static struct page *
httpd_output_read_page(struct httpd_output *httpd)
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
httpd_output_encoder_open(struct httpd_output *httpd,
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
	struct httpd_output *httpd = (struct httpd_output *)ao;

	return httpd_output_bind(httpd, error_r);
}

static void
httpd_output_disable(struct audio_output *ao)
{
	struct httpd_output *httpd = (struct httpd_output *)ao;

	httpd_output_unbind(httpd);
}

static bool
httpd_output_open(struct audio_output *ao, struct audio_format *audio_format,
		  GError **error)
{
	struct httpd_output *httpd = (struct httpd_output *)ao;

	g_mutex_lock(httpd->mutex);

	/* open the encoder */

	if (!httpd_output_encoder_open(httpd, audio_format, error)) {
		g_mutex_unlock(httpd->mutex);
		return false;
	}

	/* initialize other attributes */

	httpd->clients = NULL;
	httpd->clients_cnt = 0;
	httpd->timer = timer_new(audio_format);

	httpd->open = true;

	g_mutex_unlock(httpd->mutex);
	return true;
}

static void
httpd_client_delete(gpointer data, G_GNUC_UNUSED gpointer user_data)
{
	struct httpd_client *client = data;

	httpd_client_free(client);
}

static void
httpd_output_close(struct audio_output *ao)
{
	struct httpd_output *httpd = (struct httpd_output *)ao;

	g_mutex_lock(httpd->mutex);

	httpd->open = false;

	timer_free(httpd->timer);

	g_list_foreach(httpd->clients, httpd_client_delete, NULL);
	g_list_free(httpd->clients);

	if (httpd->header != NULL)
		page_unref(httpd->header);

	encoder_close(httpd->encoder);

	g_mutex_unlock(httpd->mutex);
}

void
httpd_output_remove_client(struct httpd_output *httpd,
			   struct httpd_client *client)
{
	assert(httpd != NULL);
	assert(client != NULL);

	httpd->clients = g_list_remove(httpd->clients, client);
	httpd->clients_cnt--;
}

void
httpd_output_send_header(struct httpd_output *httpd,
			 struct httpd_client *client)
{
	if (httpd->header != NULL)
		httpd_client_send(client, httpd->header);
}

static unsigned
httpd_output_delay(struct audio_output *ao)
{
	struct httpd_output *httpd = (struct httpd_output *)ao;

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

static void
httpd_client_check_queue(gpointer data, G_GNUC_UNUSED gpointer user_data)
{
	struct httpd_client *client = data;

	if (httpd_client_queue_size(client) > 256 * 1024) {
		g_debug("client is too slow, flushing its queue");
		httpd_client_cancel(client);
	}
}

static void
httpd_client_send_page(gpointer data, gpointer user_data)
{
	struct httpd_client *client = data;
	struct page *page = user_data;

	httpd_client_send(client, page);
}

/**
 * Broadcasts a page struct to all clients.
 */
static void
httpd_output_broadcast_page(struct httpd_output *httpd, struct page *page)
{
	assert(page != NULL);

	g_mutex_lock(httpd->mutex);
	g_list_foreach(httpd->clients, httpd_client_send_page, page);
	g_mutex_unlock(httpd->mutex);
}

/**
 * Broadcasts data from the encoder to all clients.
 */
static void
httpd_output_encoder_to_clients(struct httpd_output *httpd)
{
	struct page *page;

	g_mutex_lock(httpd->mutex);
	g_list_foreach(httpd->clients, httpd_client_check_queue, NULL);
	g_mutex_unlock(httpd->mutex);

	while ((page = httpd_output_read_page(httpd)) != NULL) {
		httpd_output_broadcast_page(httpd, page);
		page_unref(page);
	}
}

static bool
httpd_output_encode_and_play(struct httpd_output *httpd,
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
	struct httpd_output *httpd = (struct httpd_output *)ao;

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
	struct httpd_output *httpd = (struct httpd_output *)ao;

	if (httpd_output_lock_has_clients(httpd)) {
		static const char silence[1020];
		return httpd_output_play(ao, silence, sizeof(silence),
					 NULL) > 0;
	} else {
		return true;
	}
}

static void
httpd_send_metadata(gpointer data, gpointer user_data)
{
	struct httpd_client *client = data;
	struct page *icy_metadata = user_data;

	httpd_client_send_metadata(client, icy_metadata);
}

static void
httpd_output_tag(struct audio_output *ao, const struct tag *tag)
{
	struct httpd_output *httpd = (struct httpd_output *)ao;

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
			g_mutex_lock(httpd->mutex);
			g_list_foreach(httpd->clients,
				       httpd_send_metadata, httpd->metadata);
			g_mutex_unlock(httpd->mutex);
		}
	}
}

static void
httpd_client_cancel_callback(gpointer data, G_GNUC_UNUSED gpointer user_data)
{
	struct httpd_client *client = data;

	httpd_client_cancel(client);
}

static void
httpd_output_cancel(struct audio_output *ao)
{
	struct httpd_output *httpd = (struct httpd_output *)ao;

	g_mutex_lock(httpd->mutex);
	g_list_foreach(httpd->clients, httpd_client_cancel_callback, NULL);
	g_mutex_unlock(httpd->mutex);
}

const struct audio_output_plugin httpd_output_plugin = {
	.name = "httpd",
	.init = httpd_output_init,
	.finish = httpd_output_finish,
	.enable = httpd_output_enable,
	.disable = httpd_output_disable,
	.open = httpd_output_open,
	.close = httpd_output_close,
	.delay = httpd_output_delay,
	.send_tag = httpd_output_tag,
	.play = httpd_output_play,
	.pause = httpd_output_pause,
	.cancel = httpd_output_cancel,
};
