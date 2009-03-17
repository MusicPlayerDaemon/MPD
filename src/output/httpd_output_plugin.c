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

#include "httpd_internal.h"
#include "httpd_client.h"
#include "output_api.h"
#include "encoder_plugin.h"
#include "encoder_list.h"
#include "socket_util.h"
#include "page.h"

#include <assert.h>

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

	/* initialize listen address */

	sin = (struct sockaddr_in *)&httpd->address;
	memset(sin, 0, sizeof(sin));
	sin->sin_port = htons(port);
	sin->sin_family = AF_INET;
	sin->sin_addr.s_addr = INADDR_ANY;
	httpd->address_size = sizeof(*sin);

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
	struct httpd_client *client = httpd_client_new(httpd, fd);

	httpd->clients = g_list_prepend(httpd->clients, client);
}

static gboolean
httpd_listen_in_event(G_GNUC_UNUSED GIOChannel *source,
		      G_GNUC_UNUSED GIOCondition condition,
		      gpointer data)
{
	struct httpd_output *httpd = data;
	int fd;
	struct sockaddr_storage sa;
	socklen_t sa_length = sizeof(sa);

	g_mutex_lock(httpd->mutex);

	/* the listener socket has become readable - a client has
	   connected */

	fd = accept(httpd->fd, (struct sockaddr*)&sa, &sa_length);
	if (fd >= 0)
		httpd_client_add(httpd, fd);
	else if (fd < 0 && errno != EINTR)
		g_warning("accept() failed: %s", g_strerror(errno));

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
httpd_output_open(void *data, struct audio_format *audio_format,
		  GError **error)
{
	struct httpd_output *httpd = data;
	bool success;
	GIOChannel *channel;

	g_mutex_lock(httpd->mutex);

	/* create and set up listener socket */

	httpd->fd = socket_bind_listen(PF_INET, SOCK_STREAM, 0,
				       (struct sockaddr *)&httpd->address,
				       httpd->address_size,
				       16, error);
	if (httpd->fd < 0) {
		g_mutex_unlock(httpd->mutex);
		return false;
	}

	channel = g_io_channel_unix_new(httpd->fd);
	httpd->source_id = g_io_add_watch(channel, G_IO_IN,
					  httpd_listen_in_event, httpd);
	g_io_channel_unref(channel);

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
	httpd->timer = timer_new(audio_format);

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

	timer_free(httpd->timer);

	g_list_foreach(httpd->clients, httpd_client_delete, NULL);
	g_list_free(httpd->clients);

	if (httpd->header != NULL)
		page_unref(httpd->header);

	encoder_close(httpd->encoder);

	g_source_remove(httpd->source_id);
	close(httpd->fd);

	g_mutex_unlock(httpd->mutex);
}

void
httpd_output_remove_client(struct httpd_output *httpd,
			   struct httpd_client *client)
{
	assert(httpd != NULL);
	assert(client != NULL);

	httpd->clients = g_list_remove(httpd->clients, client);
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

static bool
httpd_output_encode_and_play(struct httpd_output *httpd,
			     const void *chunk, size_t size, GError **error)
{
	bool success;
	struct page *page;

	success = encoder_write(httpd->encoder, chunk, size, error);
	if (!success)
		return false;

	g_mutex_lock(httpd->mutex);
	g_list_foreach(httpd->clients, httpd_client_check_queue, page);
	g_mutex_unlock(httpd->mutex);

	while ((page = httpd_output_read_page(httpd)) != NULL) {
		g_mutex_lock(httpd->mutex);

		g_list_foreach(httpd->clients,
			       httpd_client_send_page, page);

		g_mutex_unlock(httpd->mutex);
		page_unref(page);
	}

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
httpd_output_tag(void *data, const struct tag *tag)
{
	struct httpd_output *httpd = data;

	/* XXX add suport for icy-metadata */

	encoder_tag(httpd->encoder, tag, NULL);
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
	.open = httpd_output_open,
	.close = httpd_output_close,
	.send_tag = httpd_output_tag,
	.play = httpd_output_play,
	.cancel = httpd_output_cancel,
};
