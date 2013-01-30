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
#include "Page.hxx"
#include "IcyMetaDataServer.hxx"
#include "fd_util.h"
#include "event/ServerSocket.hxx"
#include "Main.hxx"

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

static void
httpd_listen_in_event(int fd, const struct sockaddr *address,
		      size_t address_length, int uid, void *ctx);

inline
HttpdOutput::HttpdOutput(EventLoop &_loop)
	:encoder(nullptr), unflushed_input(0),
	 server_socket(new ServerSocket(_loop, httpd_listen_in_event, this)),
	 metadata(nullptr)
{
}

HttpdOutput::~HttpdOutput()
{
	if (metadata != nullptr)
		metadata->Unref();

	if (encoder != nullptr)
		encoder_finish(encoder);

	delete server_socket;

}

inline bool
HttpdOutput::Bind(GError **error_r)
{
	open = false;

	const ScopeLock protect(mutex);
	return server_socket->Open(error_r);
}

inline void
HttpdOutput::Unbind()
{
	assert(!open);

	const ScopeLock protect(mutex);
	server_socket->Close();
}

inline bool
HttpdOutput::Configure(const config_param *param, GError **error_r)
{
	/* read configuration */
	name = config_get_block_string(param, "name", "Set name in config");
	genre = config_get_block_string(param, "genre", "Set genre in config");
	website = config_get_block_string(param, "website",
					  "Set website in config");

	guint port = config_get_block_unsigned(param, "port", 8000);

	const char *encoder_name =
		config_get_block_string(param, "encoder", "vorbis");
	const struct encoder_plugin *encoder_plugin =
		encoder_plugin_get(encoder_name);
	if (encoder_plugin == NULL) {
		g_set_error(error_r, httpd_output_quark(), 0,
			    "No such encoder: %s", encoder_name);
		return false;
	}

	clients_max = config_get_block_unsigned(param,"max_clients", 0);

	/* set up bind_to_address */

	const char *bind_to_address =
		config_get_block_string(param, "bind_to_address", NULL);
	bool success = bind_to_address != NULL &&
		strcmp(bind_to_address, "any") != 0
		? server_socket->AddHost(bind_to_address, port, error_r)
		: server_socket->AddPort(port, error_r);
	if (!success)
		return false;

	/* initialize encoder */

	encoder = encoder_init(encoder_plugin, param, error_r);
	if (encoder == nullptr)
		return false;

	/* determine content type */
	content_type = encoder_get_mime_type(encoder);
	if (content_type == nullptr)
		content_type = "application/octet-stream";

	return true;
}

static struct audio_output *
httpd_output_init(const struct config_param *param,
		  GError **error_r)
{
	HttpdOutput *httpd = new HttpdOutput(*main_loop);

	if (!ao_base_init(&httpd->base, &httpd_output_plugin, param,
			  error_r)) {
		delete httpd;
		return nullptr;
	}

	if (!httpd->Configure(param, error_r)) {
		ao_base_finish(&httpd->base);
		delete httpd;
		return nullptr;
	}

	return &httpd->base;
}

static inline constexpr HttpdOutput *
Cast(audio_output *ao)
{
	return (HttpdOutput *)((char *)ao - offsetof(HttpdOutput, base));
}

static void
httpd_output_finish(struct audio_output *ao)
{
	HttpdOutput *httpd = Cast(ao);

	ao_base_finish(&httpd->base);
	delete httpd;
}

/**
 * Creates a new #HttpdClient object and adds it into the
 * HttpdOutput.clients linked list.
 */
inline void
HttpdOutput::AddClient(int fd)
{
	clients.emplace_front(this, fd,
			      encoder->plugin->tag == NULL);
	++clients_cnt;

	/* pass metadata to client */
	if (metadata != nullptr)
		clients.front().PushMetaData(metadata);
}

static void
httpd_listen_in_event(int fd, const struct sockaddr *address,
		      size_t address_length, G_GNUC_UNUSED int uid, void *ctx)
{
	HttpdOutput *httpd = (HttpdOutput *)ctx;

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
			httpd->AddClient(fd);
		else
			close_socket(fd);
	} else if (fd < 0 && errno != EINTR) {
		g_warning("accept() failed: %s", g_strerror(errno));
	}
}

Page *
HttpdOutput::ReadPage()
{
	if (unflushed_input >= 65536) {
		/* we have fed a lot of input into the encoder, but it
		   didn't give anything back yet - flush now to avoid
		   buffer underruns */
		encoder_flush(encoder, NULL);
		unflushed_input = 0;
	}

	size_t size = 0;
	do {
		size_t nbytes = encoder_read(encoder,
					     buffer + size,
					     sizeof(buffer) - size);
		if (nbytes == 0)
			break;

		unflushed_input = 0;

		size += nbytes;
	} while (size < sizeof(buffer));

	if (size == 0)
		return NULL;

	return Page::Copy(buffer, size);
}

static bool
httpd_output_enable(struct audio_output *ao, GError **error_r)
{
	HttpdOutput *httpd = Cast(ao);

	return httpd->Bind(error_r);
}

static void
httpd_output_disable(struct audio_output *ao)
{
	HttpdOutput *httpd = Cast(ao);

	httpd->Unbind();
}

inline bool
HttpdOutput::OpenEncoder(struct audio_format *audio_format, GError **error)
{
	if (!encoder_open(encoder, audio_format, error))
		return false;

	/* we have to remember the encoder header, i.e. the first
	   bytes of encoder output after opening it, because it has to
	   be sent to every new client */
	header = ReadPage();

	unflushed_input = 0;

	return true;
}

inline bool
HttpdOutput::Open(struct audio_format *audio_format, GError **error_r)
{
	assert(!open);
	assert(clients.empty());

	/* open the encoder */

	if (!OpenEncoder(audio_format, error_r))
		return false;

	/* initialize other attributes */

	clients_cnt = 0;
	timer = timer_new(audio_format);

	open = true;

	return true;
}

static bool
httpd_output_open(struct audio_output *ao, struct audio_format *audio_format,
		  GError **error)
{
	HttpdOutput *httpd = Cast(ao);

	assert(httpd->clients.empty());

	const ScopeLock protect(httpd->mutex);
	return httpd->Open(audio_format, error);
}

inline void
HttpdOutput::Close()
{
	assert(open);

	open = false;

	timer_free(timer);

	clients.clear();

	if (header != NULL)
		header->Unref();

	encoder_close(encoder);
}

static void
httpd_output_close(struct audio_output *ao)
{
	HttpdOutput *httpd = Cast(ao);

	const ScopeLock protect(httpd->mutex);
	httpd->Close();
}

void
HttpdOutput::RemoveClient(HttpdClient &client)
{
	assert(clients_cnt > 0);

	for (auto prev = clients.before_begin(), i = std::next(prev);;
	     prev = i, i = std::next(prev)) {
		assert(i != clients.end());
		if (&*i == &client) {
			clients.erase_after(prev);
			clients_cnt--;
			break;
		}
	}
}

void
HttpdOutput::SendHeader(HttpdClient &client) const
{
	if (header != NULL)
		client.PushPage(header);
}

static unsigned
httpd_output_delay(struct audio_output *ao)
{
	HttpdOutput *httpd = Cast(ao);

	if (!httpd->LockHasClients() && httpd->base.pause) {
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

void
HttpdOutput::BroadcastPage(Page *page)
{
	assert(page != NULL);

	const ScopeLock protect(mutex);
	for (auto &client : clients)
		client.PushPage(page);
}

void
HttpdOutput::BroadcastFromEncoder()
{
	mutex.lock();
	for (auto &client : clients) {
		if (client.GetQueueSize() > 256 * 1024) {
			g_debug("client is too slow, flushing its queue");
			client.CancelQueue();
		}
	}
	mutex.unlock();

	Page *page;
	while ((page = ReadPage()) != nullptr) {
		BroadcastPage(page);
		page->Unref();
	}
}

inline bool
HttpdOutput::EncodeAndPlay(const void *chunk, size_t size, GError **error_r)
{
	if (!encoder_write(encoder, chunk, size, error_r))
		return false;

	unflushed_input += size;

	BroadcastFromEncoder();
	return true;
}

static size_t
httpd_output_play(struct audio_output *ao, const void *chunk, size_t size,
		  GError **error_r)
{
	HttpdOutput *httpd = Cast(ao);

	if (httpd->LockHasClients()) {
		if (!httpd->EncodeAndPlay(chunk, size, error_r))
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
	HttpdOutput *httpd = Cast(ao);

	if (httpd->LockHasClients()) {
		static const char silence[1020] = { 0 };
		return httpd_output_play(ao, silence, sizeof(silence),
					 NULL) > 0;
	} else {
		return true;
	}
}

inline void
HttpdOutput::SendTag(const struct tag *tag)
{
	assert(tag != NULL);

	if (encoder->plugin->tag != NULL) {
		/* embed encoder tags */

		/* flush the current stream, and end it */

		encoder_pre_tag(encoder, NULL);
		BroadcastFromEncoder();

		/* send the tag to the encoder - which starts a new
		   stream now */

		encoder_tag(encoder, tag, NULL);

		/* the first page generated by the encoder will now be
		   used as the new "header" page, which is sent to all
		   new clients */

		Page *page = ReadPage();
		if (page != NULL) {
			if (header != NULL)
				header->Unref();
			header = page;
			BroadcastPage(page);
		}
	} else {
		/* use Icy-Metadata */

		if (metadata != NULL)
			metadata->Unref();

		static constexpr tag_type types[] = {
			TAG_ALBUM, TAG_ARTIST, TAG_TITLE,
			TAG_NUM_OF_ITEM_TYPES
		};

		metadata = icy_server_metadata_page(tag, &types[0]);
		if (metadata != NULL) {
			const ScopeLock protect(mutex);
			for (auto &client : clients)
				client.PushMetaData(metadata);
		}
	}
}

static void
httpd_output_tag(struct audio_output *ao, const struct tag *tag)
{
	HttpdOutput *httpd = Cast(ao);

	httpd->SendTag(tag);
}

static void
httpd_output_cancel(struct audio_output *ao)
{
	HttpdOutput *httpd = Cast(ao);

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
