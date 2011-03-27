#include <glib.h>
#include <despotify.h>

#include "tag.h"
#include "conf.h"
#include "despotify_utils.h"

static struct despotify_session *g_session;
static void (*registered_callbacks[8])(struct despotify_session *,
		int, void *, void *);
static void *registered_callback_data[8];

static void callback(struct despotify_session* ds, int sig,
		void* data, G_GNUC_UNUSED void* callback_data)
{
	size_t i;

	for (i = 0; i < sizeof(registered_callbacks) / sizeof(registered_callbacks[0]); i++) {
		void (*cb)(struct despotify_session *, int, void *, void *) = registered_callbacks[i];
		void *cb_data = registered_callback_data[i];

		if (cb)
			cb(ds, sig, data, cb_data);
	}
}

bool mpd_despotify_register_callback(void (*cb)(struct despotify_session *, int, void *, void *),
		void *cb_data)
{
	size_t i;

	for (i = 0; i < sizeof(registered_callbacks) / sizeof(registered_callbacks[0]); i++) {

		if (!registered_callbacks[i]) {
			registered_callbacks[i] = cb;
			registered_callback_data[i] = cb_data;

			return true;
		}
	}

	return false;
}

void mpd_despotify_unregister_callback(void (*cb)(struct despotify_session *, int, void *, void *))
{
	size_t i;

	for (i = 0; i < sizeof(registered_callbacks) / sizeof(registered_callbacks[0]); i++) {

		if (registered_callbacks[i] == cb) {
			registered_callbacks[i] = NULL;
		}
	}
}


struct tag *mpd_despotify_tag_from_track(struct ds_track *track)
{
	char tracknum[20];
	char comment[80];
	char date[20];
	struct tag *tag;

	tag = tag_new();

	if (!track->has_meta_data)
		return tag;

	g_snprintf(tracknum, sizeof(tracknum), "%d", track->tracknumber);
	g_snprintf(date, sizeof(date), "%d", track->year);
	g_snprintf(comment, sizeof(comment), "Bitrate %d Kbps, %sgeo restricted",
			track->file_bitrate / 1000, track->geo_restricted ? "" : "not ");
	tag_add_item(tag, TAG_TITLE, track->title);
	tag_add_item(tag, TAG_ARTIST, track->artist->name);
	tag_add_item(tag, TAG_TRACK, tracknum);
	tag_add_item(tag, TAG_ALBUM, track->album);
	tag_add_item(tag, TAG_DATE, date);
	tag_add_item(tag, TAG_COMMENT, comment);
	tag->time = track->length / 1000;

	return tag;
}

struct despotify_session *mpd_despotify_get_session(void)
{
	const char *user;
	const char *passwd;
	bool high_bitrate;

	if (g_session)
		return g_session;

	user = config_get_string(CONF_DESPOTIFY_USER, NULL);
	passwd = config_get_string(CONF_DESPOTIFY_PASSWORD, NULL);
	high_bitrate = config_get_bool(CONF_DESPOTIFY_HIGH_BITRATE, true);

	if (user == NULL || passwd == NULL) {
		g_debug("disabling despotify because account is not configured");
		return NULL;
	}
		if (!despotify_init()) {
		g_debug("Can't initialize despotify\n");
		return false;
	}

	g_session = despotify_init_client(callback, NULL,
			high_bitrate, true);
	if (!g_session) {
		g_debug("Can't initialize despotify client\n");
		return false;
	}

    if (!despotify_authenticate(g_session, user, passwd)) {
    	g_debug("Can't authenticate despotify session\n");
    	despotify_exit(g_session);
    	return false;
    }

	return g_session;
}
