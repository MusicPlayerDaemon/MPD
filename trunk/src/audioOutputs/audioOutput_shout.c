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

#include "../audioOutput.h"

#include <stdlib.h>

#ifdef HAVE_SHOUT

#include "../conf.h"
#include "../log.h"
#include "../pcm_utils.h"

#include <string.h>
#include <time.h>

#include <shout/shout.h>
#include <vorbis/vorbisenc.h>

#define CONN_ATTEMPT_INTERVAL	60

static int shoutInitCount;

/* lots of this code blatantly stolent from bossogg/bossao2 */

typedef struct _ShoutData {
	shout_t *shoutConn;
	int shoutError;

	ogg_stream_state os;
	ogg_page og;
	ogg_packet op;
	ogg_packet header_main;
	ogg_packet header_comments;
	ogg_packet header_codebooks;

	vorbis_dsp_state vd;
	vorbis_block vb;
	vorbis_info vi;
	vorbis_comment vc;

	float quality;
	int bitrate;

	int opened;

	MpdTag *tag;
	int tagToSend;

	int connAttempts;
	time_t lastAttempt;
	int last_err;

	/* just a pointer to audioOutput->outAudioFormat */
	AudioFormat *audioFormat;
} ShoutData;

static ShoutData *newShoutData(void)
{
	ShoutData *ret = xmalloc(sizeof(ShoutData));

	ret->shoutConn = shout_new();
	ret->opened = 0;
	ret->tag = NULL;
	ret->tagToSend = 0;
	ret->bitrate = -1;
	ret->quality = -2.0;
	ret->connAttempts = 0;
	ret->lastAttempt = 0;
	ret->audioFormat = NULL;
	ret->last_err = SHOUTERR_UNCONNECTED;

	return ret;
}

static void freeShoutData(ShoutData * sd)
{
	if (sd->shoutConn)
		shout_free(sd->shoutConn);
	if (sd->tag)
		freeMpdTag(sd->tag);

	free(sd);
}

#define checkBlockParam(name) { \
	blockParam = getBlockParam(param, name); \
	if (!blockParam) { \
		FATAL("no \"%s\" defined for shout device defined at line " \
				"%i\n", name, param->line); \
	} \
}

static int myShout_initDriver(AudioOutput * audioOutput, ConfigParam * param)
{
	ShoutData *sd;
	char *test;
	int port;
	char *host;
	char *mount;
	char *passwd;
	char *user;
	char *name;
	BlockParam *blockParam;
	unsigned int public = 0;

	sd = newShoutData();

	if (shoutInitCount == 0)
		shout_init();

	shoutInitCount++;

	checkBlockParam("host");
	host = blockParam->value;

	checkBlockParam("mount");
	mount = blockParam->value;

	checkBlockParam("port");

	port = strtol(blockParam->value, &test, 10);

	if (*test != '\0' || port <= 0) {
		FATAL("shout port \"%s\" is not a positive integer, line %i\n",
		      blockParam->value, blockParam->line);
	}

	checkBlockParam("password");
	passwd = blockParam->value;

	checkBlockParam("name");
	name = blockParam->value;

	blockParam = getBlockParam(param, "public");
	if (blockParam) {
		if (0 == strcmp(blockParam->value, "yes")) {
			public = 1;
		} else if (0 == strcmp(blockParam->value, "no")) {
			public = 0;
		} else {
			FATAL("public \"%s\" is not \"yes\" or \"no\" at line "
			      "%i\n", param->value, param->line);
		}
	}

	blockParam = getBlockParam(param, "user");
	if (blockParam)
		user = blockParam->value;
	else
		user = "source";

	blockParam = getBlockParam(param, "quality");

	if (blockParam) {
		int line = blockParam->line;

		sd->quality = strtod(blockParam->value, &test);

		if (*test != '\0' || sd->quality < -1.0 || sd->quality > 10.0) {
			FATAL("shout quality \"%s\" is not a number in the "
			      "range -1 to 10, line %i\n", blockParam->value,
			      blockParam->line);
		}

		blockParam = getBlockParam(param, "bitrate");

		if (blockParam) {
			FATAL("quality (line %i) and bitrate (line %i) are "
			      "both defined for shout output\n", line,
			      blockParam->line);
		}
	} else {
		blockParam = getBlockParam(param, "bitrate");

		if (!blockParam) {
			FATAL("neither bitrate nor quality defined for shout "
			      "output at line %i\n", param->line);
		}

		sd->bitrate = strtol(blockParam->value, &test, 10);

		if (*test != '\0' || sd->bitrate <= 0) {
			FATAL("bitrate at line %i should be a positive integer "
			      "\n", blockParam->line);
		}
	}

	checkBlockParam("format");
	sd->audioFormat = &audioOutput->outAudioFormat;

	if (shout_set_host(sd->shoutConn, host) != SHOUTERR_SUCCESS ||
	    shout_set_port(sd->shoutConn, port) != SHOUTERR_SUCCESS ||
	    shout_set_password(sd->shoutConn, passwd) != SHOUTERR_SUCCESS ||
	    shout_set_mount(sd->shoutConn, mount) != SHOUTERR_SUCCESS ||
	    shout_set_name(sd->shoutConn, name) != SHOUTERR_SUCCESS ||
	    shout_set_user(sd->shoutConn, user) != SHOUTERR_SUCCESS ||
	    shout_set_public(sd->shoutConn, public) != SHOUTERR_SUCCESS ||
	    shout_set_nonblocking(sd->shoutConn, 1) != SHOUTERR_SUCCESS ||
	    shout_set_format(sd->shoutConn, SHOUT_FORMAT_VORBIS)
	    != SHOUTERR_SUCCESS ||
	    shout_set_protocol(sd->shoutConn, SHOUT_PROTOCOL_HTTP)
	    != SHOUTERR_SUCCESS ||
	    shout_set_agent(sd->shoutConn, "MPD") != SHOUTERR_SUCCESS) {
		FATAL("error configuring shout defined at line %i: %s\n",
		      param->line, shout_get_error(sd->shoutConn));
	}

	/* optional paramters */
	blockParam = getBlockParam(param, "genre");
	if (blockParam && shout_set_genre(sd->shoutConn, blockParam->value)) {
		FATAL("error configuring shout defined at line %i: %s\n",
		      param->line, shout_get_error(sd->shoutConn));
	}

	blockParam = getBlockParam(param, "description");
	if (blockParam && shout_set_description(sd->shoutConn,
						blockParam->value)) {
		FATAL("error configuring shout defined at line %i: %s\n",
		      param->line, shout_get_error(sd->shoutConn));
	}

	{
		char temp[11];
		memset(temp, 0, sizeof(temp));

		snprintf(temp, sizeof(temp), "%d", sd->audioFormat->channels);
		shout_set_audio_info(sd->shoutConn, SHOUT_AI_CHANNELS, temp);

		snprintf(temp, sizeof(temp), "%d", sd->audioFormat->sampleRate);

		shout_set_audio_info(sd->shoutConn, SHOUT_AI_SAMPLERATE, temp);

		if (sd->quality >= -1.0) {
			snprintf(temp, sizeof(temp), "%2.2f", sd->quality);
			shout_set_audio_info(sd->shoutConn, SHOUT_AI_QUALITY,
					     temp);
		} else {
			snprintf(temp, sizeof(temp), "%d", sd->bitrate);
			shout_set_audio_info(sd->shoutConn, SHOUT_AI_BITRATE,
					     temp);
		}
	}

	audioOutput->data = sd;

	return 0;
}

static int myShout_handleError(ShoutData * sd, int err)
{
	switch (err) {
	case SHOUTERR_SUCCESS:
		break;
	case SHOUTERR_UNCONNECTED:
	case SHOUTERR_SOCKET:
		ERROR("Lost shout connection to %s:%i : %s\n",
		      shout_get_host(sd->shoutConn),
		      shout_get_port(sd->shoutConn),
		      shout_get_error(sd->shoutConn));
		sd->shoutError = 1;
		return -1;
	default:
		ERROR("shout: connection to %s:%i error : %s\n",
		      shout_get_host(sd->shoutConn),
		      shout_get_port(sd->shoutConn),
		      shout_get_error(sd->shoutConn));
		sd->shoutError = 1;
		return -1;
	}

	return 0;
}

static int write_page(ShoutData * sd)
{
	int err = 0;

	/*DEBUG("shout_delay: %i\n", shout_delay(sd->shoutConn)); */
	shout_sync(sd->shoutConn);
	err = shout_send(sd->shoutConn, sd->og.header, sd->og.header_len);
	if (myShout_handleError(sd, err) < 0)
		return -1;
	err = shout_send(sd->shoutConn, sd->og.body, sd->og.body_len);
	if (myShout_handleError(sd, err) < 0)
		return -1;

	return 0;
}

static void finishEncoder(ShoutData * sd)
{
	vorbis_analysis_wrote(&sd->vd, 0);

	while (vorbis_analysis_blockout(&sd->vd, &sd->vb) == 1) {
		vorbis_analysis(&sd->vb, NULL);
		vorbis_bitrate_addblock(&sd->vb);
		while (vorbis_bitrate_flushpacket(&sd->vd, &sd->op)) {
			ogg_stream_packetin(&sd->os, &sd->op);
		}
	}
}

static int flushEncoder(ShoutData * sd)
{
	return (ogg_stream_pageout(&sd->os, &sd->og) > 0);
}

static void clearEncoder(ShoutData * sd)
{
	finishEncoder(sd);
	while (1 == flushEncoder(sd)) {
		if (!sd->shoutError)
			write_page(sd);
	}

	vorbis_comment_clear(&sd->vc);
	ogg_stream_clear(&sd->os);
	vorbis_block_clear(&sd->vb);
	vorbis_dsp_clear(&sd->vd);
	vorbis_info_clear(&sd->vi);
}

static void myShout_closeShoutConn(ShoutData * sd)
{
	if (sd->opened) {
		clearEncoder(sd);

		if (shout_close(sd->shoutConn) != SHOUTERR_SUCCESS) {
			ERROR("problem closing connection to shout server: "
			      "%s\n", shout_get_error(sd->shoutConn));
		}
	}

	sd->last_err = SHOUTERR_UNCONNECTED;
	sd->opened = 0;
}

static void myShout_finishDriver(AudioOutput * audioOutput)
{
	ShoutData *sd = (ShoutData *) audioOutput->data;

	myShout_closeShoutConn(sd);

	freeShoutData(sd);

	shoutInitCount--;

	if (shoutInitCount == 0)
		shout_shutdown();
}

static void myShout_dropBufferedAudio(AudioOutput * audioOutput)
{
	/* needs to be implemented */
}

static void myShout_closeDevice(AudioOutput * audioOutput)
{
	ShoutData *sd = (ShoutData *) audioOutput->data;

	myShout_closeShoutConn(sd);

	audioOutput->open = 0;
}

#define addTag(name, value) { \
	if(value) vorbis_comment_add_tag(&(sd->vc), name, value); \
}

static void copyTagToVorbisComment(ShoutData * sd)
{
	if (sd->tag) {
		int i;

		for (i = 0; i < sd->tag->numOfItems; i++) {
			switch (sd->tag->items[i].type) {
			case TAG_ITEM_ARTIST:
				addTag("ARTIST", sd->tag->items[i].value);
				break;
			case TAG_ITEM_ALBUM:
				addTag("ALBUM", sd->tag->items[i].value);
				break;
			case TAG_ITEM_TITLE:
				addTag("TITLE", sd->tag->items[i].value);
				break;
			}
		}
	}
}

static int initEncoder(ShoutData * sd)
{
	vorbis_info_init(&(sd->vi));

	if (sd->quality >= -1.0) {
		if (0 != vorbis_encode_init_vbr(&(sd->vi),
						sd->audioFormat->channels,
						sd->audioFormat->sampleRate,
						sd->quality * 0.1)) {
			ERROR("problem setting up vorbis encoder for shout\n");
			vorbis_info_clear(&(sd->vi));
			return -1;
		}
	} else {
		if (0 != vorbis_encode_init(&(sd->vi),
					    sd->audioFormat->channels,
					    sd->audioFormat->sampleRate, -1.0,
					    sd->bitrate * 1000, -1.0)) {
			ERROR("problem setting up vorbis encoder for shout\n");
			vorbis_info_clear(&(sd->vi));
			return -1;
		}
	}

	vorbis_analysis_init(&(sd->vd), &(sd->vi));
	vorbis_block_init(&(sd->vd), &(sd->vb));

	ogg_stream_init(&(sd->os), rand());

	vorbis_comment_init(&(sd->vc));

	return 0;
}

static int myShout_openShoutConn(AudioOutput * audioOutput)
{
	ShoutData *sd = (ShoutData *) audioOutput->data;
	time_t t = time(NULL);

	if (sd->connAttempts != 0 &&
	    (t - sd->lastAttempt) < CONN_ATTEMPT_INTERVAL) {
		return -1;
	}

	sd->connAttempts++;

	if (sd->last_err == SHOUTERR_UNCONNECTED)
		sd->last_err = shout_open(sd->shoutConn);
	switch (sd->last_err) {
	case SHOUTERR_SUCCESS:
	case SHOUTERR_CONNECTED:
		break;
	case SHOUTERR_BUSY:
		sd->last_err = shout_get_connected(sd->shoutConn);
		if (sd->last_err == SHOUTERR_CONNECTED)
			break;
		return -1;
	default:
		sd->lastAttempt = t;
		ERROR("problem opening connection to shout server %s:%i "
		      "(attempt %i): %s\n",
		      shout_get_host(sd->shoutConn),
		      shout_get_port(sd->shoutConn),
		      sd->connAttempts, shout_get_error(sd->shoutConn));
		return -1;
	}

	if (initEncoder(sd) < 0) {
		shout_close(sd->shoutConn);
		return -1;
	}

	sd->shoutError = 0;

	copyTagToVorbisComment(sd);

	vorbis_analysis_headerout(&(sd->vd), &(sd->vc), &(sd->header_main),
				  &(sd->header_comments),
				  &(sd->header_codebooks));

	ogg_stream_packetin(&(sd->os), &(sd->header_main));
	ogg_stream_packetin(&(sd->os), &(sd->header_comments));
	ogg_stream_packetin(&(sd->os), &(sd->header_codebooks));

	sd->opened = 1;
	sd->tagToSend = 0;

	while (ogg_stream_flush(&(sd->os), &(sd->og))) {
		if (write_page(sd) < 0) {
			myShout_closeShoutConn(sd);
			return -1;
		}
	}

	sd->connAttempts = 0;

	return 0;
}

static int myShout_openDevice(AudioOutput * audioOutput)
{
	ShoutData *sd = (ShoutData *) audioOutput->data;

	audioOutput->open = 1;

	if (sd->opened)
		return 0;

	if (myShout_openShoutConn(audioOutput) < 0) {
		audioOutput->open = 0;
		return -1;
	}

	return 0;
}

static void myShout_sendMetadata(ShoutData * sd)
{
	if (!sd->opened || !sd->tag)
		return;

	clearEncoder(sd);
	if (initEncoder(sd) < 0)
		return;

	copyTagToVorbisComment(sd);

	vorbis_analysis_headerout(&(sd->vd), &(sd->vc), &(sd->header_main),
				  &(sd->header_comments),
				  &(sd->header_codebooks));

	ogg_stream_packetin(&(sd->os), &(sd->header_main));
	ogg_stream_packetin(&(sd->os), &(sd->header_comments));
	ogg_stream_packetin(&(sd->os), &(sd->header_codebooks));

	while (ogg_stream_flush(&(sd->os), &(sd->og))) {
		if (write_page(sd) < 0) {
			myShout_closeShoutConn(sd);
			return;
		}
	}

	/*if(sd->tag) freeMpdTag(sd->tag);
	   sd->tag = NULL; */
	sd->tagToSend = 0;
}

static int myShout_play(AudioOutput * audioOutput, char *playChunk, int size)
{
	int i, j;
	ShoutData *sd = (ShoutData *) audioOutput->data;
	float **vorbbuf;
	int samples;
	int bytes = sd->audioFormat->bits / 8;

	if (sd->opened && sd->tagToSend)
		myShout_sendMetadata(sd);

	if (!sd->opened) {
		if (myShout_openShoutConn(audioOutput) < 0) {
			return -1;
		}
	}

	samples = size / (bytes * sd->audioFormat->channels);

	/* this is for only 16-bit audio */

	vorbbuf = vorbis_analysis_buffer(&(sd->vd), samples);

	for (i = 0; i < samples; i++) {
		for (j = 0; j < sd->audioFormat->channels; j++) {
			vorbbuf[j][i] = (*((mpd_sint16 *) playChunk)) / 32768.0;
			playChunk += bytes;
		}
	}

	vorbis_analysis_wrote(&(sd->vd), samples);

	while (1 == vorbis_analysis_blockout(&(sd->vd), &(sd->vb))) {
		vorbis_analysis(&(sd->vb), NULL);
		vorbis_bitrate_addblock(&(sd->vb));

		while (vorbis_bitrate_flushpacket(&(sd->vd), &(sd->op))) {
			ogg_stream_packetin(&(sd->os), &(sd->op));
		}
	}

	while (ogg_stream_pageout(&(sd->os), &(sd->og)) != 0) {
		if (write_page(sd) < 0) {
			myShout_closeShoutConn(sd);
			return -1;
		}
	}

	return 0;
}

static void myShout_setTag(AudioOutput * audioOutput, MpdTag * tag)
{
	ShoutData *sd = (ShoutData *) audioOutput->data;

	if (sd->tag)
		freeMpdTag(sd->tag);
	sd->tag = NULL;
	sd->tagToSend = 0;

	if (!tag)
		return;

	sd->tag = mpdTagDup(tag);
	sd->tagToSend = 1;
}

AudioOutputPlugin shoutPlugin = {
	"shout",
	NULL,
	myShout_initDriver,
	myShout_finishDriver,
	myShout_openDevice,
	myShout_play,
	myShout_dropBufferedAudio,
	myShout_closeDevice,
	myShout_setTag,
};

#else

DISABLED_AUDIO_OUTPUT_PLUGIN(shoutPlugin)
#endif
