/*
 * Copyright 2003-2016 The Music Player Daemon Project
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
#include "FlacDecoderPlugin.h"
#include "FlacStreamDecoder.hxx"
#include "FlacDomain.hxx"
#include "FlacCommon.hxx"
#include "FlacMetadata.hxx"
#include "OggCodec.hxx"
#include "fs/Path.hxx"
#include "fs/NarrowPath.hxx"
#include "util/Error.hxx"
#include "Log.hxx"

#if !defined(FLAC_API_VERSION_CURRENT) || FLAC_API_VERSION_CURRENT <= 7
#error libFLAC is too old
#endif

static void flacPrintErroredState(FLAC__StreamDecoderState state)
{
	switch (state) {
	case FLAC__STREAM_DECODER_SEARCH_FOR_METADATA:
	case FLAC__STREAM_DECODER_READ_METADATA:
	case FLAC__STREAM_DECODER_SEARCH_FOR_FRAME_SYNC:
	case FLAC__STREAM_DECODER_READ_FRAME:
	case FLAC__STREAM_DECODER_END_OF_STREAM:
		return;

	case FLAC__STREAM_DECODER_OGG_ERROR:
	case FLAC__STREAM_DECODER_SEEK_ERROR:
	case FLAC__STREAM_DECODER_ABORTED:
	case FLAC__STREAM_DECODER_MEMORY_ALLOCATION_ERROR:
	case FLAC__STREAM_DECODER_UNINITIALIZED:
		break;
	}

	LogError(flac_domain, FLAC__StreamDecoderStateString[state]);
}

static void flacMetadata(gcc_unused const FLAC__StreamDecoder * dec,
			 const FLAC__StreamMetadata * block, void *vdata)
{
	flac_metadata_common_cb(block, (FlacDecoder *) vdata);
}

static FLAC__StreamDecoderWriteStatus
flac_write_cb(const FLAC__StreamDecoder *dec, const FLAC__Frame *frame,
	      const FLAC__int32 *const buf[], void *vdata)
{
	FlacDecoder *data = (FlacDecoder *) vdata;
	FLAC__uint64 nbytes = 0;

	if (FLAC__stream_decoder_get_decode_position(dec, &nbytes)) {
		if (data->position > 0 && nbytes > data->position) {
			nbytes -= data->position;
			data->position += nbytes;
		} else {
			data->position = nbytes;
			nbytes = 0;
		}
	} else
		nbytes = 0;

	return flac_common_write(data, frame, buf, nbytes);
}

static bool
flac_scan_file(Path path_fs,
	       const TagHandler &handler, void *handler_ctx)
{
	FlacMetadataChain chain;
	if (!chain.Read(NarrowPath(path_fs))) {
		FormatDebug(flac_domain,
			    "Failed to read FLAC tags: %s",
			    chain.GetStatusString());
		return false;
	}

	chain.Scan(handler, handler_ctx);
	return true;
}

static bool
flac_scan_stream(InputStream &is,
		 const TagHandler &handler, void *handler_ctx)
{
	FlacMetadataChain chain;
	if (!chain.Read(is)) {
		FormatDebug(flac_domain,
			    "Failed to read FLAC tags: %s",
			    chain.GetStatusString());
		return false;
	}

	chain.Scan(handler, handler_ctx);
	return true;
}

/**
 * Some glue code around FLAC__stream_decoder_new().
 */
static FlacStreamDecoder
flac_decoder_new(void)
{
	FlacStreamDecoder sd;
	if (!sd) {
		LogError(flac_domain,
			 "FLAC__stream_decoder_new() failed");
		return sd;
	}

	if(!FLAC__stream_decoder_set_metadata_respond(sd.get(), FLAC__METADATA_TYPE_VORBIS_COMMENT))
		LogDebug(flac_domain,
			 "FLAC__stream_decoder_set_metadata_respond() has failed");

	return sd;
}

static bool
flac_decoder_initialize(FlacDecoder *data, FLAC__StreamDecoder *sd)
{
	if (!FLAC__stream_decoder_process_until_end_of_metadata(sd)) {
		if (FLAC__stream_decoder_get_state(sd) != FLAC__STREAM_DECODER_END_OF_STREAM)
			LogWarning(flac_domain, "problem reading metadata");
		return false;
	}

	if (data->initialized) {
		/* done */
		return true;
	}

	if (data->GetInputStream().IsSeekable())
		/* allow the workaround below only for nonseekable
		   streams*/
		return false;

	/* no stream_info packet found; try to initialize the decoder
	   from the first frame header */
	FLAC__stream_decoder_process_single(sd);
	return data->initialized;
}

static void
flac_decoder_loop(FlacDecoder *data, FLAC__StreamDecoder *flac_dec)
{
	Decoder &decoder = *data->GetDecoder();

	while (true) {
		DecoderCommand cmd;
		if (!data->tag.IsEmpty()) {
			cmd = decoder_tag(decoder, data->GetInputStream(),
					  std::move(data->tag));
			data->tag.Clear();
		} else
			cmd = decoder_get_command(decoder);

		if (cmd == DecoderCommand::SEEK) {
			FLAC__uint64 seek_sample =
				decoder_seek_where_frame(decoder);
			if (FLAC__stream_decoder_seek_absolute(flac_dec, seek_sample)) {
				data->position = 0;
				decoder_command_finished(decoder);
			} else
				decoder_seek_error(decoder);
		} else if (cmd == DecoderCommand::STOP)
			break;

		switch (FLAC__stream_decoder_get_state(flac_dec)) {
		case FLAC__STREAM_DECODER_SEARCH_FOR_METADATA:
		case FLAC__STREAM_DECODER_READ_METADATA:
		case FLAC__STREAM_DECODER_SEARCH_FOR_FRAME_SYNC:
		case FLAC__STREAM_DECODER_READ_FRAME:
			/* continue decoding */
			break;

		case FLAC__STREAM_DECODER_END_OF_STREAM:
			/* regular end of stream */
			return;

		case FLAC__STREAM_DECODER_SEEK_ERROR:
			/* try to recover from seek error */
			if (!FLAC__stream_decoder_flush(flac_dec)) {
				LogError(flac_domain, "FLAC__stream_decoder_flush() failed");
				return;
			}

			break;

		case FLAC__STREAM_DECODER_OGG_ERROR:
		case FLAC__STREAM_DECODER_ABORTED:
		case FLAC__STREAM_DECODER_MEMORY_ALLOCATION_ERROR:
			/* an error, fatal enough for us to abort the
			   decoder */
			return;

		case FLAC__STREAM_DECODER_UNINITIALIZED:
			/* we shouldn't see this, ever - bail out */
			return;
		}

		if (!FLAC__stream_decoder_process_single(flac_dec) &&
		    decoder_get_command(decoder) == DecoderCommand::NONE) {
			/* a failure that was not triggered by a
			   decoder command */
			flacPrintErroredState(FLAC__stream_decoder_get_state(flac_dec));
			break;
		}
	}
}

static FLAC__StreamDecoderInitStatus
stream_init_oggflac(FLAC__StreamDecoder *flac_dec, FlacDecoder *data)
{
	return FLAC__stream_decoder_init_ogg_stream(flac_dec,
						    FlacInput::Read,
						    FlacInput::Seek,
						    FlacInput::Tell,
						    FlacInput::Length,
						    FlacInput::Eof,
						    flac_write_cb,
						    flacMetadata,
						    FlacInput::Error,
						    data);
}

static FLAC__StreamDecoderInitStatus
stream_init_flac(FLAC__StreamDecoder *flac_dec, FlacDecoder *data)
{
	return FLAC__stream_decoder_init_stream(flac_dec,
						FlacInput::Read,
						FlacInput::Seek,
						FlacInput::Tell,
						FlacInput::Length,
						FlacInput::Eof,
						flac_write_cb,
						flacMetadata,
						FlacInput::Error,
						data);
}

static FLAC__StreamDecoderInitStatus
stream_init(FLAC__StreamDecoder *flac_dec, FlacDecoder *data, bool is_ogg)
{
	return is_ogg
		? stream_init_oggflac(flac_dec, data)
		: stream_init_flac(flac_dec, data);
}

static bool
FlacInitAndDecode(FlacDecoder &data, FLAC__StreamDecoder *sd, bool is_ogg)
{
	auto init_status = stream_init(sd, &data, is_ogg);
	if (init_status != FLAC__STREAM_DECODER_INIT_STATUS_OK) {
		LogWarning(flac_domain,
			   FLAC__StreamDecoderInitStatusString[init_status]);
		return false;
	}

	bool result = flac_decoder_initialize(&data, sd);
	if (result)
		flac_decoder_loop(&data, sd);

	FLAC__stream_decoder_finish(sd);
	return result;
}

static void
flac_decode_internal(Decoder &decoder,
		     InputStream &input_stream,
		     bool is_ogg)
{
	auto flac_dec = flac_decoder_new();
	if (!flac_dec)
		return;

	FlacDecoder data(decoder, input_stream);

	FlacInitAndDecode(data, flac_dec.get(), is_ogg);
}

static void
flac_decode(Decoder &decoder, InputStream &input_stream)
{
	flac_decode_internal(decoder, input_stream, false);
}

static bool
oggflac_init(gcc_unused const ConfigBlock &block)
{
	return !!FLAC_API_SUPPORTS_OGG_FLAC;
}

static bool
oggflac_scan_file(Path path_fs,
		  const TagHandler &handler, void *handler_ctx)
{
	FlacMetadataChain chain;
	if (!chain.ReadOgg(NarrowPath(path_fs))) {
		FormatDebug(flac_domain,
			    "Failed to read OggFLAC tags: %s",
			    chain.GetStatusString());
		return false;
	}

	chain.Scan(handler, handler_ctx);
	return true;
}

static bool
oggflac_scan_stream(InputStream &is,
		    const TagHandler &handler, void *handler_ctx)
{
	FlacMetadataChain chain;
	if (!chain.ReadOgg(is)) {
		FormatDebug(flac_domain,
			    "Failed to read OggFLAC tags: %s",
			    chain.GetStatusString());
		return false;
	}

	chain.Scan(handler, handler_ctx);
	return true;
}

static void
oggflac_decode(Decoder &decoder, InputStream &input_stream)
{
	if (ogg_codec_detect(&decoder, input_stream) != OGG_CODEC_FLAC)
		return;

	/* rewind the stream, because ogg_codec_detect() has
	   moved it */
	input_stream.LockRewind(IgnoreError());

	flac_decode_internal(decoder, input_stream, true);
}

static const char *const oggflac_suffixes[] = { "ogg", "oga", nullptr };
static const char *const oggflac_mime_types[] = {
	"application/ogg",
	"application/x-ogg",
	"audio/ogg",
	"audio/x-flac+ogg",
	"audio/x-ogg",
	nullptr
};

const struct DecoderPlugin oggflac_decoder_plugin = {
	"oggflac",
	oggflac_init,
	nullptr,
	oggflac_decode,
	nullptr,
	oggflac_scan_file,
	oggflac_scan_stream,
	nullptr,
	oggflac_suffixes,
	oggflac_mime_types,
};

static const char *const flac_suffixes[] = { "flac", nullptr };
static const char *const flac_mime_types[] = {
	"application/flac",
	"application/x-flac",
	"audio/flac",
	"audio/x-flac",
	nullptr
};

const struct DecoderPlugin flac_decoder_plugin = {
	"flac",
	nullptr,
	nullptr,
	flac_decode,
	nullptr,
	flac_scan_file,
	flac_scan_stream,
	nullptr,
	flac_suffixes,
	flac_mime_types,
};
