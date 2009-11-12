/*
 * Copyright (C) 2003-2009 The Music Player Daemon Project
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
#include "httpd_internal.h"
#include "httpd_client.h"
#include "output_api.h"
#include "encoder_plugin.h"
#include "encoder_list.h"
#include "socket_util.h"
#include "page.h"
#include "icy_server.h"
#include "fd_util.h"

#include <assert.h>

#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>

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

static gboolean
httpd_listen_in_event(G_GNUC_UNUSED GIOChannel *source,
		      G_GNUC_UNUSED GIOCondition condition,
		      gpointer data);

static bool
httpd_output_bind(struct httpd_output *httpd, GError **error_r)
{
	GIOChannel *channel;

	httpd->open = false;

	/* create and set up listener socket */

	httpd->fd = socket_bind_listen(PF_INET, SOCK_STREAM, 0,
				       (struct sockaddr *)&httpd->address,
				       httpd->address_size,
				       16, error_r);
	if (httpd->fd < 0)
		return false;

	g_mutex_lock(httpd->mutex);

	channel = g_io_channel_unix_new(httpd->fd);
	httpd->source_id = g_io_add_watch(channel, G_IO_IN,
					  httpd_listen_in_event, httpd);
	g_io_channel_unref(channel);

	g_mutex_unlock(httpd->mutex);

	return true;
}

static void
httpd_output_unbind(struct httpd_output *httpd)
{
	assert(!httpd->open);

	g_mutex_lock(httpd->mutex);

	g_source_remove(httpd->source_id);
	close(httpd->fd);

	g_mutex_unlock(httpd->mutex);
}

static void *
httpd_output_init(G_GNUC_UNUSED const struct audio_format *audio_format,
		  const struct config_param *param,
		  GError **error)
{
	struct httpd_output *httpd = g_new(struct httpd_output, 1);
	const char *encoder_name;
	const struct encoder_plugin *encoder_plugin;
	guint port;
	struct sockaddr_in *sin;

	/* read configuration */

	port = config_get_block_unsigned(param, "port", 8000);

	encoder_name = config_get_block_string(param, "encoder", "vorbis");
	encoder_plugin = encoder_plugin_get(encoder_name);
	if (encoder_plugin == NULL) {
		g_set_error(error, httpd_output_quark(), 0,
			    "No such encoder: %s", encoder_name);
		return NULL;
	}

	if (strcmp(encoder_name, "vorbis") == 0)
		httpd->content_type = "application/x-ogg";
	else if (strcmp(encoder_name, "lame") == 0)
		httpd->content_type = "audio/mpeg";
	else
		httpd->content_type = "application/octet-stream";

	httpd->clients_max = config_get_block_unsigned(param,"max_clients", 0);

	/* initialize listen address */

	sin = (struct sockaddr_in *)&httpd->address;
	memset(sin, 0, sizeof(sin));
	sin->sin_port = htons(port);
	sin->sin_family = AF_INET;
	sin->sin_addr.s_addr = INADDR_ANY;
	httpd->address_size = sizeof(*sin);

	/* initialize metadata */
	httpd->metadata = NULL;

	/* initialize encoder */

	httpd->encoder = encoder_init(encoder_plugin, param, error);
	if (httpd->encoder == NULL)
		return NULL;

	httpd->mutex = g_mutex_new();

	return httpd;
}

static void
httpd_output_finish(void *data)
{
	struct httpd_output *httpd = data;

	if (httpd->metadata)
		page_unref(httpd->metadata);

	encoder_finish(httpd->encoder);
	g_mutex_free(httpd->mutex);
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

static gboolean
httpd_listen_in_event(G_GNUC_UNUSED GIOChannel *source,
		      G_GNUC_UNUSED GIOCondition condition,
		      gpointer data)
{
	struct httpd_output *httpd = data;
	int fd;
	struct sockaddr_storage sa;
	size_t sa_length = sizeof(sa);

	g_mutex_lock(httpd->mutex);

	/* the listener socket has become readable - a client has
	   connected */

	fd = accept_cloexec_nonblock(httpd->fd, (struct sockaddr*)&sa,
				     &sa_length);
	if (fd >= 0) {
		/* can we allow additional client */
		if (httpd->open &&
		    (httpd->clients_max == 0 ||
		     httpd->clients_cnt < httpd->clients_max))
			httpd_client_add(httpd, fd);
		else
			close(fd);
	} else if (fd < 0 && errno != EINTR) {
		g_warning("accept() failed: %s", g_strerror(errno));
	}

	g_mutex_unlock(httpd->mutex);

	return true;
}

/**
 * Reads data from the encoder (as much as available) and returns it
 * as a new #page object.
 */
static struct page *
httpd_output_read_page(struct httpd_output *httpd)
{
	size_t size = 0, nbytes;

	do {
		nbytes = encoder_read(httpd->encoder, httpd->buffer + size,
				      sizeof(httpd->buffer) - size);
		if (nbytes == 0)
			break;

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
	bool success;

	success = encoder_open(httpd->encoder, audio_format, error);
	if (!success)
		return false;

	/* we have to remember the encoder header, i.e. the first
	   bytes of encoder output after opening it, because it has to
	   be sent to every new client */
	httpd->header = httpd_output_read_page(httpd);
	return true;
}

static bool
httpd_output_enable(void *data, GError **error_r)
{
	struct httpd_output *httpd = data;

	return httpd_output_bind(httpd, error_r);
}

static void
httpd_output_disable(void *data)
{
	struct httpd_output *httpd = data;

	httpd_output_unbind(httpd);
}

static bool
httpd_output_open(void *data, struct audio_format *audio_format,
		  GError **error)
{
	struct httpd_output *httpd = data;
	bool success;

	g_mutex_lock(httpd->mutex);

	/* open the encoder */

	success = httpd_output_encoder_open(httpd, audio_format, error);
	if (!success) {
		g_source_remove(httpd->source_id);
		close(httpd->fd);
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

static void httpd_output_close(void *data)
{
	struct httpd_output *httpd = data;

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
	bool success;

	success = encoder_write(httpd->encoder, chunk, size, error);
	if (!success)
		return false;

	httpd_output_encoder_to_clients(httpd);

	return true;
}

static size_t
httpd_output_play(void *data, const void *chunk, size_t size, GError **error)
{
	struct httpd_output *httpd = data;
	bool has_clients;

	g_mutex_lock(httpd->mutex);
	has_clients = httpd->clients != NULL;
	g_mutex_unlock(httpd->mutex);

	if (has_clients) {
		bool success;

		success = httpd_output_encode_and_play(httpd, chunk, size,
						       error);
		if (!success)
			return 0;
	}

	if (!httpd->timer->started)
		timer_start(httpd->timer);
	else
		timer_sync(httpd->timer);
	timer_add(httpd->timer, size);

	return size;
}

static void
httpd_send_metadata(gpointer data, gpointer user_data)
{
	struct httpd_client *client = data;
	struct page *icy_metadata = user_data;

	httpd_client_send_metadata(client, icy_metadata);
}

static void
httpd_output_tag(void *data, const struct tag *tag)
{
	struct httpd_output *httpd = data;

	assert(tag != NULL);

	if (httpd->encoder->plugin->tag != NULL) {
		/* embed encoder tags */
		struct page *page;

		/* flush the current stream, and end it */

		encoder_flush(httpd->encoder, NULL);
		httpd_output_encoder_to_clients(httpd);

		/* send the tag to the encoder - which starts a new
		   stream now */

		encoder_tag(httpd->encoder, tag, NULL);

		/* the first page generated by the encoder will now be
		   used as the new "header" page, which is sent to all
		   new clients */

		page = httpd_output_read_page(httpd);
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
httpd_output_cancel(void *data)
{
	struct httpd_output *httpd = data;

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
	.send_tag = httpd_output_tag,
	.play = httpd_output_play,
	.cancel = httpd_output_cancel,
};
