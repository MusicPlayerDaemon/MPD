/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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

#include "config.h" /* must be first for large file support */
#include "Mp4v2DecoderPlugin.hxx"
#include "../DecoderAPI.hxx"
#include "CheckAudioFormat.hxx"
#include "tag/TagHandler.hxx"
#include "fs/Path.hxx"
#include "util/Error.hxx"
#include "util/Domain.hxx"
#include "Log.hxx"

#include <mp4v2/mp4v2.h>
#include <neaacdec.h>

#include <cstdio>
#include <cstdlib>

static constexpr Domain mp4v2_decoder_domain("mp4v2");

static MP4TrackId
mp4_get_aac_track(MP4FileHandle handle, NeAACDecHandle decoder,
		  AudioFormat &audio_format, Error &error)
{
	uint32_t sample_rate;
#ifdef HAVE_FAAD_LONG
	/* neaacdec.h declares all arguments as "unsigned long", but
	   internally expects uint32_t pointers.  To avoid gcc
	   warnings, use this workaround. */
	unsigned long *sample_rate_r = (unsigned long*)&sample_rate;
#else
	uint32_t *sample_rate_r = sample_rate;
#endif

	const MP4TrackId tracks = MP4GetNumberOfTracks(handle);

	for (MP4TrackId id = 1; id <= tracks; id++) {
		const char* track_type = MP4GetTrackType(handle, id);

		if (track_type == 0)
			continue;

		const auto obj_type = MP4GetTrackEsdsObjectTypeId(handle, id);

		if (obj_type == MP4_INVALID_AUDIO_TYPE)
			continue;
		if (obj_type == MP4_MPEG4_AUDIO_TYPE) {
			const auto mpeg_type = MP4GetTrackAudioMpeg4Type(handle, id);
			if (!MP4_IS_MPEG4_AAC_AUDIO_TYPE(mpeg_type))
				continue;
		} else if (!MP4_IS_AAC_AUDIO_TYPE(obj_type))
			continue;

		if (decoder == nullptr)
			/* found audio track, no decoder */
			return id;

		unsigned char *buff = nullptr;
		unsigned buff_size = 0;

		if (!MP4GetTrackESConfiguration(handle, id, &buff, &buff_size))
			continue;

		uint8_t channels;
		int32_t nbytes = NeAACDecInit(decoder, buff, buff_size,
				       sample_rate_r, &channels);

		free(buff);

		if (nbytes < 0)
			/* invalid stream */
			continue;

		if (!audio_format_init_checked(audio_format, sample_rate,
					       SampleFormat::S16,
					       channels,
					       error))
			continue;

		return id;
	}

	error.Set(mp4v2_decoder_domain, "no valid aac track found");

	return MP4_INVALID_TRACK_ID;
}

static NeAACDecHandle
mp4_faad_new(MP4FileHandle handle, AudioFormat &audio_format, Error &error)
{
	const NeAACDecHandle decoder = NeAACDecOpen();
	const NeAACDecConfigurationPtr config =
		NeAACDecGetCurrentConfiguration(decoder);
	config->outputFormat = FAAD_FMT_16BIT;
	config->downMatrix = 1;
	config->dontUpSampleImplicitSBR = 0;
	NeAACDecSetConfiguration(decoder, config);

	const auto track = mp4_get_aac_track(handle, decoder, audio_format, error);

	if (track == MP4_INVALID_TRACK_ID) {
		NeAACDecClose(decoder);
		return nullptr;
	}

	return decoder;
}

static void
mp4_file_decode(Decoder &mpd_decoder, Path path_fs)
{
	const MP4FileHandle handle = MP4Read(path_fs.c_str());

	if (handle == MP4_INVALID_FILE_HANDLE) {
		FormatError(mp4v2_decoder_domain,
			  "unable to open file");
		return;
	}

	AudioFormat audio_format;
	Error error;
	const NeAACDecHandle decoder = mp4_faad_new(handle, audio_format, error);

	if (decoder == nullptr) {
		LogError(error);
		MP4Close(handle);
		return;
	}

	const MP4TrackId track = mp4_get_aac_track(handle, nullptr, audio_format, error);

	/* initialize the MPD core */

	const MP4Timestamp scale = MP4GetTrackTimeScale(handle, track);
	const float duration = ((float)MP4GetTrackDuration(handle, track)) / scale + 0.5f;
	const MP4SampleId num_samples = MP4GetTrackNumberOfSamples(handle, track);

	decoder_initialized(mpd_decoder, audio_format, true, duration);

	/* the decoder loop */

	DecoderCommand cmd = DecoderCommand::NONE;

	for (MP4SampleId sample = 1;
	     sample < num_samples && cmd != DecoderCommand::STOP;
	     sample++) {
		unsigned char *data = nullptr;
		unsigned int data_length = 0;

		if (cmd == DecoderCommand::SEEK) {
			const MP4Timestamp offset =
				decoder_seek_time(mpd_decoder).ToScale(scale);

			sample = MP4GetSampleIdFromTime(handle, track, offset,
							false);
			decoder_command_finished(mpd_decoder);
		}

		/* read */
		if (MP4ReadSample(handle, track, sample, &data, &data_length) == 0) {
			FormatError(mp4v2_decoder_domain, "unable to read sample");
			break;
		}

		/* decode it */
		NeAACDecFrameInfo frame_info;
		const void *const decoded = NeAACDecDecode(decoder, &frame_info, data, data_length);

		if (frame_info.error > 0) {
			FormatWarning(mp4v2_decoder_domain,
				      "error decoding AAC stream: %s",
				      NeAACDecGetErrorMessage(frame_info.error));
			break;
		}

		if (frame_info.channels != audio_format.channels) {
			FormatDefault(mp4v2_decoder_domain,
				      "channel count changed from %u to %u",
				      audio_format.channels, frame_info.channels);
			break;
		}

		if (frame_info.samplerate != audio_format.sample_rate) {
			FormatDefault(mp4v2_decoder_domain,
				      "sample rate changed from %u to %lu",
				      audio_format.sample_rate,
				      (unsigned long)frame_info.samplerate);
			break;
		}

		/* update bit rate and position */
		unsigned bit_rate = 0;

		if (frame_info.samples > 0) {
			bit_rate = frame_info.bytesconsumed * 8.0 *
			    frame_info.channels * audio_format.sample_rate /
			    frame_info.samples / 1000 + 0.5;
		}

		/* send PCM samples to MPD */

		cmd = decoder_data(mpd_decoder, nullptr, decoded,
				   (size_t)frame_info.samples * 2,
				   bit_rate);

		free(data);
	}

	/* cleanup */
	NeAACDecClose(decoder);
	MP4Close(handle);
}

static inline void
mp4_safe_invoke_tag(const struct tag_handler *handler, void *handler_ctx,
		    TagType tag, const char *value)
{
	if (value != nullptr)
		tag_handler_invoke_tag(handler, handler_ctx, tag, value);
}

static bool
mp4_scan_file(Path path_fs,
		 const struct tag_handler *handler, void *handler_ctx)
{
	const MP4FileHandle handle = MP4Read(path_fs.c_str());

	if (handle == MP4_INVALID_FILE_HANDLE)
		return false;

	AudioFormat tmp_audio_format;
	Error error;
	const MP4TrackId id = mp4_get_aac_track(handle, nullptr, tmp_audio_format, error);

	if (id == MP4_INVALID_TRACK_ID) {
		LogError(error);
		MP4Close(handle);
		return false;
	}

	const MP4Duration dur = MP4GetTrackDuration(handle, id) /
		MP4GetTrackTimeScale(handle, id);
	tag_handler_invoke_duration(handler, handler_ctx, dur);

	const MP4Tags* tags = MP4TagsAlloc();
	MP4TagsFetch(tags, handle);

	mp4_safe_invoke_tag(handler, handler_ctx, TAG_NAME, tags->name);
	mp4_safe_invoke_tag(handler, handler_ctx, TAG_ARTIST, tags->artist);
	mp4_safe_invoke_tag(handler, handler_ctx, TAG_ALBUM_ARTIST, tags->albumArtist);
	mp4_safe_invoke_tag(handler, handler_ctx, TAG_ALBUM, tags->album);
	mp4_safe_invoke_tag(handler, handler_ctx, TAG_COMPOSER, tags->composer);
	mp4_safe_invoke_tag(handler, handler_ctx, TAG_COMMENT, tags->comments);
	mp4_safe_invoke_tag(handler, handler_ctx, TAG_GENRE, tags->genre);
	mp4_safe_invoke_tag(handler, handler_ctx, TAG_DATE, tags->releaseDate);
	mp4_safe_invoke_tag(handler, handler_ctx, TAG_ARTIST_SORT, tags->sortArtist);
	mp4_safe_invoke_tag(handler, handler_ctx, TAG_ALBUM_ARTIST_SORT, tags->sortAlbumArtist);

	char buff[8]; /* tmp buffer for index to string. */
	if (tags->track != nullptr) {
		sprintf(buff, "%d", tags->track->index);
		tag_handler_invoke_tag(handler, handler_ctx, TAG_TRACK, buff);
	}

	if (tags->disk != nullptr) {
		sprintf(buff, "%d", tags->disk->index);
		tag_handler_invoke_tag(handler, handler_ctx, TAG_DISC, buff);
	}

	MP4TagsFree(tags);
	MP4Close(handle);

	return true;
}

static const char *const mp4_suffixes[] = {
	"mp4",
	"m4a",
	/* "m4p", encrypted */
	/* "m4b", audio book */
	/* "m4r", ring tones */
	/* "m4v", video */
	nullptr
};

static const char *const mp4_mime_types[] = {
	"application/mp4",
	"application/m4a",
	"audio/mp4",
	"audio/m4a",
	/* "audio/m4p", */
	/* "audio/m4b", */
	/* "audio/m4r", */
	/* "audio/m4v", */
	nullptr
};

const struct DecoderPlugin mp4v2_decoder_plugin = {
	"mp4v2",
	nullptr,
	nullptr,
	nullptr,
	mp4_file_decode,
	mp4_scan_file,
	nullptr,
	nullptr,
	mp4_suffixes,
	mp4_mime_types
};
