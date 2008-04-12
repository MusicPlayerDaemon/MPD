/* the Music Player Daemon (MPD)
 * Copyright (C) 2003-2007 by Warren Dukes (warren.dukes@gmail.com)
 * This project's homepage is: http://www.musicpd.org
 * 
 * WavPack support added by Laszlo Ashin <kodest@gmail.com>
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

#include "../inputPlugin.h"

#ifdef HAVE_WAVPACK

#include "../utils.h"
#include "../audio.h"
#include "../log.h"
#include "../pcm_utils.h"
#include "../playerData.h"
#include "../outputBuffer.h"
#include "../os_compat.h"

#include <wavpack/wavpack.h>

#define ERRORLEN 80

static struct {
	const char *name;
	int type;
} tagtypes[] = {
	{ "artist",     TAG_ITEM_ARTIST },
	{ "album",      TAG_ITEM_ALBUM },
	{ "title",      TAG_ITEM_TITLE },
	{ "track",      TAG_ITEM_TRACK },
	{ "name",       TAG_ITEM_NAME },
	{ "genre",      TAG_ITEM_GENRE },
	{ "date",       TAG_ITEM_DATE },
	{ "composer",   TAG_ITEM_COMPOSER },
	{ "performer",  TAG_ITEM_PERFORMER },
	{ "comment",    TAG_ITEM_COMMENT },
	{ "disc",       TAG_ITEM_DISC },
	{ NULL,         0 }
};

/*
 * This function has been borrowed from the tiny player found on
 * wavpack.com. Modifications were required because mpd only handles
 * max 16 bit samples.
 */
static void format_samples_int(int Bps, void *buffer, uint32_t samcnt)
{
	int32_t temp;
	uchar *dst = (uchar *)buffer;
	int32_t *src = (int32_t *)buffer;

	switch (Bps) {
		case 1:
			while (samcnt--)
				*dst++ = *src++;
			break;
		case 2:
			while (samcnt--) {
				temp = *src++;
#ifdef WORDS_BIGENDIAN
				*dst++ = (uchar)(temp >> 8);
				*dst++ = (uchar)(temp); 
#else
				*dst++ = (uchar)(temp); 
				*dst++ = (uchar)(temp >> 8);
#endif
			}
			break;
		case 3:
			/* downscale to 16 bits */
			while (samcnt--) {
				temp = *src++;
#ifdef WORDS_BIGENDIAN
				*dst++ = (uchar)(temp >> 16);
				*dst++ = (uchar)(temp >> 8);
#else
				*dst++ = (uchar)(temp >> 8);
				*dst++ = (uchar)(temp >> 16);
#endif
			}
			break;
		case 4:
			/* downscale to 16 bits */
			while (samcnt--) {
				temp = *src++;
#ifdef WORDS_BIGENDIAN
				*dst++ = (uchar)(temp >> 24);
				*dst++ = (uchar)(temp >> 16);

#else
				*dst++ = (uchar)(temp >> 16);
				*dst++ = (uchar)(temp >> 24);
#endif
			}
			break;
	}
}

/*
 * This function converts floating point sample data to 16 bit integer.
 */
static void format_samples_float(int Bps, void *buffer, uint32_t samcnt)
{
	int16_t *dst = (int16_t *)buffer;
	float *src = (float *)buffer;

	while (samcnt--) {
		*dst++ = (int16_t)(*src++);
	}
}

/*
 * This does the main decoding thing.
 * Requires an already opened WavpackContext.
 */
static void wavpack_decode(OutputBuffer *cb, DecoderControl *dc,
                           WavpackContext *wpc, int canseek,
                           ReplayGainInfo *replayGainInfo)
{
	void (*format_samples)(int Bps, void *buffer, uint32_t samcnt);
	char chunk[CHUNK_SIZE];
	float file_time;
	int samplesreq, samplesgot;
	int allsamples;
	int position, outsamplesize;
	int Bps;

	dc->audioFormat.sampleRate = WavpackGetSampleRate(wpc);
	dc->audioFormat.channels = WavpackGetReducedChannels(wpc);
	dc->audioFormat.bits = WavpackGetBitsPerSample(wpc);

	if (dc->audioFormat.bits > 16)
		dc->audioFormat.bits = 16;

	if ((WavpackGetMode(wpc) & MODE_FLOAT) == MODE_FLOAT)
		format_samples = format_samples_float;
	else
		format_samples = format_samples_int;
/*
	if ((WavpackGetMode(wpc) & MODE_WVC) == MODE_WVC)
		ERROR("decoding WITH wvc!!!\n");
	else
		ERROR("decoding without wvc\n");
*/
	allsamples = WavpackGetNumSamples(wpc);
	Bps = WavpackGetBytesPerSample(wpc);

	outsamplesize = Bps;
	if (outsamplesize > 2)
		outsamplesize = 2;
	outsamplesize *= dc->audioFormat.channels;

	samplesreq = sizeof(chunk) / (4 * dc->audioFormat.channels);

	getOutputAudioFormat(&(dc->audioFormat), &(cb->audioFormat));

	dc->totalTime = (float)allsamples / dc->audioFormat.sampleRate;
	dc->state = DECODE_STATE_DECODE;
	dc->seekable = canseek;

	position = 0;

	do {
		if (dc->seek) {
			if (canseek) {
				int where;

				clearOutputBuffer(cb);

				where = dc->seekWhere *
				        dc->audioFormat.sampleRate;
				if (WavpackSeekSample(wpc, where))
					position = where;
				else
					dc->seekError = 1;
			} else {
				dc->seekError = 1;
			}

			dc->seek = 0;
			decoder_wakeup_player();
		}

		if (dc->stop)
			break;

		samplesgot = WavpackUnpackSamples(wpc,
		                                  (int32_t *)chunk, samplesreq);
		if (samplesgot > 0) {
			int bitrate = (int)(WavpackGetInstantBitrate(wpc) /
			              1000 + 0.5);
			position += samplesgot;
			file_time = (float)position /
			            dc->audioFormat.sampleRate;

			format_samples(Bps, chunk,
			               samplesgot * dc->audioFormat.channels);

			sendDataToOutputBuffer(cb, NULL, dc, 0, chunk,
			                       samplesgot * outsamplesize,
			                       file_time, bitrate,
					       replayGainInfo);
		}
	} while (samplesgot == samplesreq);

	flushOutputBuffer(cb);
}

static char *wavpack_tag(WavpackContext *wpc, char *key)
{
	char *value = NULL;
	int size;

	size = WavpackGetTagItem(wpc, key, NULL, 0);
	if (size > 0) {
		size++;
		value = xmalloc(size);
		if (!value)
			return NULL;
		WavpackGetTagItem(wpc, key, value, size);
	}

	return value;
}

static ReplayGainInfo *wavpack_replaygain(WavpackContext *wpc)
{
	ReplayGainInfo *replayGainInfo;
	int found = 0;
	char *value;

	replayGainInfo = newReplayGainInfo();

	value = wavpack_tag(wpc, "replaygain_track_gain");
	if (value) {
		replayGainInfo->trackGain = atof(value);
		free(value);
		found = 1;
	}

	value = wavpack_tag(wpc, "replaygain_album_gain");
	if (value) {
		replayGainInfo->albumGain = atof(value);
		free(value);
		found = 1;
	}

	value = wavpack_tag(wpc, "replaygain_track_peak");
	if (value) {
		replayGainInfo->trackPeak = atof(value);
		free(value);
		found = 1;
	}

	value = wavpack_tag(wpc, "replaygain_album_peak");
	if (value) {
		replayGainInfo->albumPeak = atof(value);
		free(value);
		found = 1;
	}


	if (found)
		return replayGainInfo;

	freeReplayGainInfo(replayGainInfo);

	return NULL;
}

/*
 * Reads metainfo from the specified file.
 */
static MpdTag *wavpack_tagdup(char *fname)
{
	WavpackContext *wpc;
	MpdTag *tag;
	char error[ERRORLEN];
	char *s;
	int ssize;
	int i, j;

	wpc = WavpackOpenFileInput(fname, error, OPEN_TAGS, 0);
	if (wpc == NULL) {
		ERROR("failed to open WavPack file \"%s\": %s\n", fname, error);
		return NULL;
	}

	tag = newMpdTag();
	if (tag == NULL) {
		ERROR("failed to newMpdTag()\n");
		return NULL;
	}

	tag->time =
		(float)WavpackGetNumSamples(wpc) / WavpackGetSampleRate(wpc);

	ssize = 0;
	s = NULL;

	for (i = 0; tagtypes[i].name != NULL; ++i) {
		j = WavpackGetTagItem(wpc, tagtypes[i].name, NULL, 0);
		if (j > 0) {
			++j;

			if (s == NULL) {
				s = xmalloc(j);
				if (s == NULL) break;
				ssize = j;
			} else if (j > ssize) {
				char *t = (char *)xrealloc(s, j);
				if (t == NULL) break;
				ssize = j;
				s = t;
			}

			WavpackGetTagItem(wpc, tagtypes[i].name, s, j);
			addItemToMpdTag(tag, tagtypes[i].type, s);
		}
	}

	if (s != NULL)
		free(s);

	WavpackCloseFile(wpc);

	return tag;
}

/*
 * mpd InputStream <=> WavpackStreamReader wrapper callbacks
 */

/* This struct is needed for per-stream last_byte storage. */
typedef struct {
	InputStream *is;
	/* Needed for push_back_byte() */
	int last_byte;
} InputStreamPlus;

static int32_t read_bytes(void *id, void *data, int32_t bcount)
{
	InputStreamPlus *isp = (InputStreamPlus *)id;
	uint8_t *buf = (uint8_t *)data;
	int32_t i = 0;

	if (isp->last_byte != EOF) {
		*buf++ = isp->last_byte;
		isp->last_byte = EOF;
		--bcount;
		++i;
	}
	return i + readFromInputStream(isp->is, buf, 1, bcount);
}

static uint32_t get_pos(void *id)
{
	return ((InputStreamPlus *)id)->is->offset;
}

static int set_pos_abs(void *id, uint32_t pos)
{
	return seekInputStream(((InputStreamPlus *)id)->is, pos, SEEK_SET);
}

static int set_pos_rel(void *id, int32_t delta, int mode)
{
	return seekInputStream(((InputStreamPlus *)id)->is, delta, mode);
}

static int push_back_byte(void *id, int c)
{
	((InputStreamPlus *)id)->last_byte = c;
	return 1;
}

static uint32_t get_length(void *id)
{
	return ((InputStreamPlus *)id)->is->size;
}

static int can_seek(void *id)
{
	return ((InputStreamPlus *)id)->is->seekable;
}

static WavpackStreamReader mpd_is_reader = {
	.read_bytes = read_bytes,
	.get_pos = get_pos,
	.set_pos_abs = set_pos_abs,
	.set_pos_rel = set_pos_rel,
	.push_back_byte = push_back_byte,
	.get_length = get_length,
	.can_seek = can_seek,
	.write_bytes = NULL /* no need to write edited tags */
};

static void
initInputStreamPlus(InputStreamPlus *isp, InputStream *is)
{
	isp->is = is;
	isp->last_byte = EOF;
}

/*
 * Tries to decode the specified stream, and gives true if managed to do it.
 */
static unsigned int wavpack_trydecode(InputStream *is)
{
	char error[ERRORLEN];
	WavpackContext *wpc;
	InputStreamPlus isp;

	initInputStreamPlus(&isp, is);
	wpc = WavpackOpenFileInputEx(&mpd_is_reader, &isp, NULL, error,
	                             OPEN_STREAMING, 0);
	if (wpc == NULL)
		return 0;

	WavpackCloseFile(wpc);
	/* Seek it back in order to play from the first byte. */
	seekInputStream(is, 0, SEEK_SET);

	return 1;
}

/*
 * Decodes a stream.
 */
static int wavpack_streamdecode(OutputBuffer *cb, DecoderControl *dc,
                                InputStream *is)
{
	char error[ERRORLEN];
	WavpackContext *wpc;
	InputStream is_wvc;
	int open_flags = OPEN_2CH_MAX | OPEN_NORMALIZE /*| OPEN_STREAMING*/;
	char *wvc_url = NULL;
	int err;
	InputStreamPlus isp, isp_wvc;
	int canseek;

	/* Try to find wvc */
	do {
		char tmp[MPD_PATH_MAX];
		const char *utf8url;
		size_t len;
		err = 1;

		/*
		 * As we use dc->utf8url, this function will be bad for
		 * single files. utf8url is not absolute file path :/
		 */
		utf8url = get_song_url(tmp, getPlayerData()->playerControl.current_song);
		if (utf8url == NULL) {
			break;
		}

		len = strlen(utf8url);
		if (!len) {
			break;
		}

		wvc_url = (char *)xmalloc(len + 2); /* +2: 'c' and EOS */
		if (wvc_url == NULL) {
			break;
		}

		memcpy(wvc_url, utf8url, len);
		wvc_url[len] = 'c';
		wvc_url[len + 1] = '\0';

		if (openInputStream(&is_wvc, wvc_url)) {
			break;
		}

		/*
		 * And we try to buffer in order to get know
		 * about a possible 404 error.
		 */
		for (;;) {
			if (inputStreamAtEOF(&is_wvc)) {
				/*
				 * EOF is reached even without
				 * a single byte is read...
				 * So, this is not good :/
				 */
				break;
			}

			if (bufferInputStream(&is_wvc) >= 0) {
				err = 0;
				break;
			}

			if (dc->stop) {
				break;
			}

			/* Save some CPU */
			my_usleep(1000);
		}
		if (err) {
			closeInputStream(&is_wvc);
			break;
		}

		open_flags |= OPEN_WVC;

	} while (0);

	canseek = can_seek(&isp);
	if (wvc_url != NULL) {
		if (err) {
			free(wvc_url);
			wvc_url = NULL;
		} else {
			initInputStreamPlus(&isp_wvc, &is_wvc);
		}
	}

	initInputStreamPlus(&isp, is);
	wpc = WavpackOpenFileInputEx(&mpd_is_reader, &isp, &isp_wvc, error,
				     open_flags, 15);

	if (wpc == NULL) {
		ERROR("failed to open WavPack stream: %s\n", error);
		return -1;
	}

	wavpack_decode(cb, dc, wpc, canseek, NULL);

	WavpackCloseFile(wpc);
	if (wvc_url != NULL) {
		closeInputStream(&is_wvc);
		free(wvc_url);
	}
	closeInputStream(is);

	return 0;
}

/*
 * Decodes a file.
 */
static int wavpack_filedecode(OutputBuffer *cb, DecoderControl *dc, char *fname)
{
	char error[ERRORLEN];
	WavpackContext *wpc;
	ReplayGainInfo *replayGainInfo;

	wpc = WavpackOpenFileInput(fname, error,
	                           OPEN_TAGS | OPEN_WVC |
	                           OPEN_2CH_MAX | OPEN_NORMALIZE, 15);
	if (wpc == NULL) {
		ERROR("failed to open WavPack file \"%s\": %s\n", fname, error);
		return -1;
	}

	replayGainInfo = wavpack_replaygain(wpc);

	wavpack_decode(cb, dc, wpc, 1, replayGainInfo);

	if (replayGainInfo)
		freeReplayGainInfo(replayGainInfo);

	WavpackCloseFile(wpc);

	return 0;
}

static char const *wavpackSuffixes[] = { "wv", NULL };
static char const *wavpackMimeTypes[] = { "audio/x-wavpack", NULL };

InputPlugin wavpackPlugin = {
	"wavpack",
	NULL,
	NULL,
	wavpack_trydecode,
	wavpack_streamdecode,
	wavpack_filedecode,
	wavpack_tagdup,
	INPUT_PLUGIN_STREAM_FILE | INPUT_PLUGIN_STREAM_URL,
	wavpackSuffixes,
	wavpackMimeTypes
};

#else /* !HAVE_WAVPACK */

InputPlugin wavpackPlugin;

#endif /* !HAVE_WAVPACK */
