#include "config.h"
#include "../decoder_api.h"
#include "audio_check.h"
#include "uri.h"
#include "tag_handler.h"

#include <glib.h>
#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <gme/gme.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "gme"

#define SUBTUNE_PREFIX "tune_"

enum {
	GME_SAMPLE_RATE = 44100,
	GME_CHANNELS = 2,
	GME_BUFFER_FRAMES = 2048,
	GME_BUFFER_SAMPLES = GME_BUFFER_FRAMES * GME_CHANNELS,
};

/**
 * returns the file path stripped of any /tune_xxx.* subtune
 * suffix
 */
static char *
get_container_name(const char *path_fs)
{
	const char *subtune_suffix = uri_get_suffix(path_fs);
	char *path_container = g_strdup(path_fs);
	char *pat = g_strconcat("*/" SUBTUNE_PREFIX "???.", subtune_suffix, NULL);
	GPatternSpec *path_with_subtune = g_pattern_spec_new(pat);
	g_free(pat);
	if (!g_pattern_match(path_with_subtune,
			     strlen(path_container), path_container, NULL)) {
		g_pattern_spec_free(path_with_subtune);
		return path_container;
	}

	char *ptr = g_strrstr(path_container, "/" SUBTUNE_PREFIX);
	if (ptr != NULL)
		*ptr='\0';

	g_pattern_spec_free(path_with_subtune);
	return path_container;
}

/**
 * returns tune number from file.nsf/tune_xxx.* style path or 0 if no subtune
 * is appended.
 */
static int
get_song_num(const char *path_fs)
{
	const char *subtune_suffix = uri_get_suffix(path_fs);
	char *pat = g_strconcat("*/" SUBTUNE_PREFIX "???.", subtune_suffix, NULL);
	GPatternSpec *path_with_subtune = g_pattern_spec_new(pat);
	g_free(pat);

	if (g_pattern_match(path_with_subtune,
			    strlen(path_fs), path_fs, NULL)) {
		char *sub = g_strrstr(path_fs, "/" SUBTUNE_PREFIX);
		g_pattern_spec_free(path_with_subtune);
		if(!sub)
			return 0;

		sub += strlen("/" SUBTUNE_PREFIX);
		int song_num = strtol(sub, NULL, 10);

		return song_num - 1;
	} else {
		g_pattern_spec_free(path_with_subtune);
		return 0;
	}
}

static char *
gme_container_scan(const char *path_fs, const unsigned int tnum)
{
	Music_Emu *emu;
	const char* gme_err;
	unsigned int num_songs;

	gme_err = gme_open_file(path_fs, &emu, GME_SAMPLE_RATE);
	if (gme_err != NULL) {
		g_warning("%s", gme_err);
		return NULL;
	}

	num_songs = gme_track_count(emu);
	/* if it only contains a single tune, don't treat as container */
	if (num_songs < 2)
		return NULL;

	const char *subtune_suffix = uri_get_suffix(path_fs);
	if (tnum <= num_songs){
		char *subtune = g_strdup_printf(
			SUBTUNE_PREFIX "%03u.%s", tnum, subtune_suffix);
		return subtune;
	} else
		return NULL;
}

static void
gme_file_decode(struct decoder *decoder, const char *path_fs)
{
	float song_len;
	Music_Emu *emu;
	gme_info_t *ti;
	struct audio_format audio_format;
	enum decoder_command cmd;
	short buf[GME_BUFFER_SAMPLES];
	const char* gme_err;
	char *path_container = get_container_name(path_fs);
	int song_num = get_song_num(path_fs);

	gme_err = gme_open_file(path_container, &emu, GME_SAMPLE_RATE);
	g_free(path_container);
	if (gme_err != NULL) {
		g_warning("%s", gme_err);
		return;
	}

	if((gme_err = gme_track_info(emu, &ti, song_num)) != NULL){
		g_warning("%s", gme_err);
		gme_delete(emu);
		return;
	}

	if(ti->length > 0)
		song_len = ti->length / 1000.0;
	else song_len = -1;

	/* initialize the MPD decoder */

	GError *error = NULL;
	if (!audio_format_init_checked(&audio_format, GME_SAMPLE_RATE,
				       SAMPLE_FORMAT_S16, GME_CHANNELS,
				       &error)) {
		g_warning("%s", error->message);
		g_error_free(error);
		gme_free_info(ti);
		gme_delete(emu);
		return;
	}

	decoder_initialized(decoder, &audio_format, true, song_len);

	if((gme_err = gme_start_track(emu, song_num)) != NULL)
		g_warning("%s", gme_err);

	if(ti->length > 0)
		gme_set_fade(emu, ti->length);

	/* play */
	do {
		gme_err = gme_play(emu, GME_BUFFER_SAMPLES, buf);
		if (gme_err != NULL) {
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

static bool
gme_scan_file(const char *path_fs,
	      const struct tag_handler *handler, void *handler_ctx)
{
	Music_Emu *emu;
	gme_info_t *ti;
	const char* gme_err;
	char *path_container=get_container_name(path_fs);
	int song_num;
	song_num=get_song_num(path_fs);

	gme_err = gme_open_file(path_container, &emu, GME_SAMPLE_RATE);
	g_free(path_container);
	if (gme_err != NULL) {
		g_warning("%s", gme_err);
		return false;
	}
	if((gme_err = gme_track_info(emu, &ti, song_num)) != NULL){
		g_warning("%s", gme_err);
		gme_delete(emu);
		return false;
	}

	assert(ti != NULL);

	if(ti->length > 0)
		tag_handler_invoke_duration(handler, handler_ctx,
					    ti->length / 100);

	if(ti->song != NULL){
		if(gme_track_count(emu) > 1){
			/* start numbering subtunes from 1 */
			char *tag_title=g_strdup_printf("%s (%d/%d)",
							ti->song, song_num+1, gme_track_count(emu));
			tag_handler_invoke_tag(handler, handler_ctx,
					       TAG_TITLE, tag_title);
			g_free(tag_title);
		}else
			tag_handler_invoke_tag(handler, handler_ctx,
					       TAG_TITLE, ti->song);
	}
	if(ti->author != NULL)
		tag_handler_invoke_tag(handler, handler_ctx,
				       TAG_ARTIST, ti->author);
	if(ti->game != NULL)
		tag_handler_invoke_tag(handler, handler_ctx,
				       TAG_ALBUM, ti->game);
	if(ti->comment != NULL)
		tag_handler_invoke_tag(handler, handler_ctx,
				       TAG_COMMENT, ti->comment);
	if(ti->copyright != NULL)
		tag_handler_invoke_tag(handler, handler_ctx,
				       TAG_DATE, ti->copyright);

	gme_free_info(ti);
	gme_delete(emu);

	return true;
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
	.scan_file = gme_scan_file,
	.suffixes = gme_suffixes,
	.container_scan = gme_container_scan,
};
