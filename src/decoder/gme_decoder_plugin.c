#include "config.h"
#include "../decoder_api.h"
#include "audio_check.h"
#include <glib.h>
#include <assert.h>
#include <gme/gme.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "gme"

#define GME_BUF_SIZE	4096

static void
gme_file_decode(struct decoder *decoder, const char *path_fs)
{
	int sample_rate = 44100;
	int track = 0; /* index of track to play */
	float song_len;
	Music_Emu *emu;
	gme_info_t *ti;
	struct audio_format audio_format;
	enum decoder_command cmd;
	short buf[GME_BUF_SIZE];
	const char* gme_err;

	if((gme_err = gme_open_file(path_fs, &emu, sample_rate)) != NULL){
		g_warning("%s", gme_err);
		return;
	}
	if((gme_err = gme_track_info(emu, &ti, 0)) != NULL){
		g_warning("%s", gme_err);
		gme_delete(emu);
		return;
	}

	if(ti->length > 0)
		song_len = ti->length / 1000.0;
	else song_len = -1;

	/* initialize the MPD decoder */

	GError *error = NULL;
	if(!audio_format_init_checked(&audio_format, sample_rate, SAMPLE_FORMAT_S16,
					  2, &error)){
		g_warning("%s", error->message);
		g_error_free(error);
		gme_free_info(ti);
		gme_delete(emu);
		return;
	}

	decoder_initialized(decoder, &audio_format, true, song_len);

	if((gme_err = gme_start_track(emu, track)) != NULL)
		g_warning("%s", gme_err);

	/* play */
	do {
		if((gme_err = gme_play(emu, GME_BUF_SIZE, buf)) != NULL){
			g_warning("%s", gme_err);
			return;
		}
		cmd = decoder_data(decoder, NULL, buf, sizeof(buf), 0);

		if(cmd == DECODE_COMMAND_SEEK) {
			float where = decoder_seek_where(decoder);
			if((gme_err = gme_seek(emu, (int)where*1000)) != NULL)
				g_warning("%s", gme_err);
			decoder_command_finished(decoder);
		}

		if(gme_track_ended(emu))
			break;
	} while(cmd != DECODE_COMMAND_STOP);

	gme_free_info(ti);
	gme_delete(emu);
}

static struct tag *
gme_tag_dup(const char *path_fs)
{
	int sample_rate = 44100;
	Music_Emu *emu;
	gme_info_t *ti;
	const char* gme_err;

	if((gme_err = gme_open_file(path_fs, &emu, sample_rate)) != NULL){
		g_warning("%s", gme_err);
		return NULL;
	}
	if((gme_err = gme_track_info(emu, &ti, 0)) != NULL){
		g_warning("%s", gme_err);
		gme_delete(emu);
		return NULL;
	}

	struct tag *tag = tag_new();
	if(ti != NULL){
		if(ti->length > 0)
			tag->time = ti->length / 1000;
		if(ti->song != NULL)
			tag_add_item(tag, TAG_TITLE, ti->song);
		if(ti->author != NULL)
			tag_add_item(tag, TAG_ARTIST, ti->author);
		if(ti->comment != NULL)
			tag_add_item(tag, TAG_COMMENT, ti->comment);
		if(ti->copyright != NULL)
			tag_add_item(tag, TAG_DATE, ti->copyright);
	}

	gme_free_info(ti);
	gme_delete(emu);
	return tag;
}

static const char *const gme_suffixes[] = {
	"ay", "gbs", "gym", "hes", "kss", "nsf",
	"nsfe", "sap", "spc", "vgm", "vgz",
	NULL
};

extern const struct decoder_plugin gme_decoder_plugin;
const struct decoder_plugin gme_decoder_plugin = {
	.name = "gme",
	.file_decode = gme_file_decode,
	.tag_dup = gme_tag_dup,
	.suffixes = gme_suffixes,
};
