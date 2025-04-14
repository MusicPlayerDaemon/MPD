// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Mpg123DecoderPlugin.hxx"
#include "../DecoderAPI.hxx"
#include "input/InputStream.hxx"
#include "pcm/CheckAudioFormat.hxx"
#include "tag/Handler.hxx"
#include "tag/Builder.hxx"
#include "tag/ReplayGainParser.hxx"
#include "tag/MixRampParser.hxx"
#include "fs/NarrowPath.hxx"
#include "fs/Path.hxx"
#include "lib/fmt/ExceptionFormatter.hxx"
#include "lib/fmt/RuntimeError.hxx"
#include "util/Domain.hxx"
#include "util/ScopeExit.hxx"
#include "Log.hxx"
#include "config.h" // for ENABLE_ID3TAG

#ifdef ENABLE_ID3TAG
#include "tag/Id3MixRamp.hxx"
#include "tag/Id3Parse.hxx"
#include "tag/Id3ReplayGain.hxx"
#include "tag/Id3Scan.hxx"
#include "tag/Id3Unique.hxx"
#else
#include "util/IterableSplitString.hxx"
#endif

#include <mpg123.h>

#include <stdio.h>

static constexpr Domain mpg123_domain("mpg123");

static bool
mpd_mpg123_init([[maybe_unused]] const ConfigBlock &block)
{
	mpg123_init();

	return true;
}

static void
mpd_mpg123_finish() noexcept
{
	mpg123_exit();
}

/**
 * Opens a file with an existing #mpg123_handle.
 *
 * @param handle a handle which was created before; on error, this
 * function will not free it
 * @return true on success
 */
static bool
mpd_mpg123_open(mpg123_handle *handle, Path path_fs)
{
	auto np = NarrowPath(path_fs);
	int error = mpg123_open(handle, np);
	if (error != MPG123_OK) {
		FmtWarning(mpg123_domain,
			   "libmpg123 failed to open {}: {}",
			   np.c_str(), mpg123_plain_strerror(error));
		return false;
	}

	return true;
}

struct mpd_mpg123_iohandle {
	DecoderClient *client;
	InputStream &is;
};

static
#if MPG123_API_VERSION >= 47
/* this typedef was added to libmpg123 somewhere between 1.26.4 (45)
   and 1.31.2 (47) */
mpg123_ssize_t
#else
ssize_t
#endif
mpd_mpg123_read(void *_iohandle, void *data, size_t size) noexcept
{
	auto &iohandle = *reinterpret_cast<mpd_mpg123_iohandle *>(_iohandle);

	try {
		return decoder_read_much(iohandle.client, iohandle.is,
					 {reinterpret_cast<std::byte *>(data), size});
	} catch (...) {
		LogError(std::current_exception(), "Read failed");
		return -1;
	}
}

static off_t
mpd_mpg123_lseek(void *_iohandle, off_t offset, int whence) noexcept
{
	auto &iohandle = *reinterpret_cast<mpd_mpg123_iohandle *>(_iohandle);

	if (whence != SEEK_SET)
		return -1;

	try {
		iohandle.is.LockSeek(offset);
		return offset;
	} catch (...) {
		LogError(std::current_exception(), "Seek failed");
		return -1;
	}
}

/**
 * Opens an #InputStream with an existing #mpg123_handle.
 *
 * Throws on error.
 *
 * @param handle a handle which was created before; on error, this
 * function will not free it
 */
static void
mpd_mpg123_open_stream(mpg123_handle &handle, mpd_mpg123_iohandle &iohandle)
{
	if (int error = mpg123_replace_reader_handle(&handle, mpd_mpg123_read, mpd_mpg123_lseek,
						     nullptr);
	    error != MPG123_OK)
		throw FmtRuntimeError("mpg123_replace_reader() failed: %s",
				      mpg123_plain_strerror(error));

	if (int error = mpg123_open_handle(&handle, &iohandle);
	    error != MPG123_OK)
		throw FmtRuntimeError("mpg123_open_handle() failed: %s",
				      mpg123_plain_strerror(error));
}

/**
 * Convert libmpg123's format to an #AudioFormat instance.
 *
 * @param handle a handle which was created before; on error, this
 * function will not free it
 * @param audio_format this parameter is filled after successful
 * return
 * @return true on success
 */
static bool
GetAudioFormat(mpg123_handle &handle, AudioFormat &audio_format)
{
	long rate;
	int channels, encoding;
	if (const int error = mpg123_getformat(&handle, &rate, &channels, &encoding);
	    error != MPG123_OK) {
		FmtWarning(mpg123_domain,
			   "mpg123_getformat() failed: {}",
			   mpg123_plain_strerror(error));
		return false;
	}

	if (encoding != MPG123_ENC_SIGNED_16) {
		/* other formats not yet implemented */
		FmtWarning(mpg123_domain,
			   "expected MPG123_ENC_SIGNED_16, got {}",
			   encoding);
		return false;
	}

	audio_format = CheckAudioFormat(rate, SampleFormat::S16, channels);
	return true;
}

#ifdef ENABLE_ID3TAG

static UniqueId3Tag
ParseId3(mpg123_handle &handle) noexcept
{
	unsigned char *data;
	size_t size;

	return mpg123_id3_raw(&handle, nullptr, nullptr, &data, &size) == MPG123_OK &&
		data != nullptr && size > 0
		? id3_tag_parse(std::span{reinterpret_cast<const std::byte *>(data), size})
		: UniqueId3Tag{};
}

#else // ENABLE_ID3TAG

static void
AddTagItem(TagBuilder &tag, TagType type, const mpg123_string &s) noexcept
{
	assert(s.p != nullptr);
	assert(s.size >= s.fill);
	assert(s.fill > 0);

	/* "fill" includes the closing null byte; strip it here */
	const std::string_view all_values{s.p, s.fill - 1};

	/* a mpg123_string can contain multiple values, each separated
	   by a null byte */
	for (const std::string_view value : IterableSplitString(all_values, '\0'))
		/* we need this "empty" check because there is no
		   value after the last null terminator */
		if (!value.empty())
			tag.AddItem(type, value);
}

static void
AddTagItem(TagBuilder &tag, TagType type, const mpg123_string *s) noexcept
{
	if (s != nullptr)
		AddTagItem(tag, type, *s);
}

static void
mpd_mpg123_id3v2_tag(DecoderClient &client, InputStream *is,
		     const mpg123_id3v2 &id3v2) noexcept
{
	TagBuilder tag;

	AddTagItem(tag, TAG_TITLE, id3v2.title);
	AddTagItem(tag, TAG_ARTIST, id3v2.artist);
	AddTagItem(tag, TAG_ALBUM, id3v2.album);
	AddTagItem(tag, TAG_DATE, id3v2.year);
	AddTagItem(tag, TAG_GENRE, id3v2.genre);

	for (size_t i = 0, n = id3v2.comments; i < n; ++i)
		AddTagItem(tag, TAG_COMMENT, id3v2.comment_list[i].text);

	client.SubmitTag(is, tag.Commit());
}

static void
mpd_mpg123_id3v2_extras(DecoderClient &client, const mpg123_id3v2 &id3v2) noexcept
{
	ReplayGainInfo replay_gain;
	replay_gain.Clear();

	MixRampInfo mix_ramp;

	bool found_replay_gain = false, found_mixramp = false;

	for (size_t i = 0, n = id3v2.extras; i < n; ++i) {
		if (ParseReplayGainTag(replay_gain,
				       id3v2.extra[i].description.p,
				       id3v2.extra[i].text.p))
			found_replay_gain = true;
		else if (ParseMixRampTag(mix_ramp,
					 id3v2.extra[i].description.p,
					 id3v2.extra[i].text.p))
			found_mixramp = true;
	}

	if (found_replay_gain)
		client.SubmitReplayGain(&replay_gain);

	if (found_mixramp)
		client.SubmitMixRamp(std::move(mix_ramp));
}

static void
mpd_mpg123_id3v2(DecoderClient &client, InputStream *is,
		 const mpg123_id3v2 &id3v2) noexcept
{
	mpd_mpg123_id3v2_tag(client, is, id3v2);
	mpd_mpg123_id3v2_extras(client, id3v2);
}

#endif // !ENABLE_ID3TAG

static void
mpd_mpg123_meta(DecoderClient &client, InputStream *is,
		mpg123_handle *const handle) noexcept
{
	if ((mpg123_meta_check(handle) & MPG123_NEW_ID3) == 0)
		return;

#ifdef ENABLE_ID3TAG
	/* get the raw ID3v2 tag from libmpg123 and parse it using
	   libid3tag (for full ID3v2 support) */

	if (const auto tag = ParseId3(*handle)) {
		client.SubmitTag(is, tag_id3_import(tag.get()));

		if (ReplayGainInfo rgi;
		    Id3ToReplayGainInfo(rgi, tag.get()))
			client.SubmitReplayGain(&rgi);

		if (auto mix_ramp = Id3ToMixRampInfo(tag.get());
		    mix_ramp.IsDefined())
			client.SubmitMixRamp(std::move(mix_ramp));
	}

	/* this call clears the MPG123_NEW_ID3 flag */
	mpg123_id3(handle, nullptr, nullptr);
#else // ENABLE_ID3TAG
	/* we don't have libid3tag: use libmpg123's built-in ID3v2
	   parser (which doesn't support all tag types) */

	mpg123_id3v2 *v2;
	if (mpg123_id3(handle, nullptr, &v2) != MPG123_OK)
		return;

	if (v2 != nullptr)
		mpd_mpg123_id3v2(client, is, *v2);
#endif // !ENABLE_ID3TAG
}

[[gnu::pure]]
static SignedSongTime
GetDuration(mpg123_handle &handle, const AudioFormat &audio_format) noexcept
{
	const off_t num_samples = mpg123_length(&handle);
	if (num_samples < 0)
		return SignedSongTime::Negative();

	return SongTime::FromScale<uint64_t>(num_samples,
					     audio_format.sample_rate);
}

static void
Decode(DecoderClient &client, InputStream *is,
       mpg123_handle &handle, const bool seekable)
{
#ifdef ENABLE_ID3TAG
	/* prepare for using mpg123_id3_raw() later */
	mpg123_param(&handle, MPG123_ADD_FLAGS, MPG123_STORE_RAW_ID3, 0);
#endif

	AudioFormat audio_format;
	if (!GetAudioFormat(handle, audio_format))
		return;

	/* tell MPD core we're ready */

	const auto duration = GetDuration(handle, audio_format);

	client.Ready(audio_format, seekable, duration);

	struct mpg123_frameinfo info;
	if (mpg123_info(&handle, &info) != MPG123_OK) {
		info.vbr = MPG123_CBR;
		info.bitrate = 0;
	}

	switch (info.vbr) {
	case MPG123_ABR:
		info.bitrate = info.abr_rate;
		break;
	case MPG123_CBR:
		break;
	default:
		info.bitrate = 0;
	}

	/* the decoder main loop */
	DecoderCommand cmd;
	do {
		/* read metadata */
		mpd_mpg123_meta(client, is, &handle);

		/* decode */

		unsigned char buffer[8192];
		size_t nbytes;
		if (int error = mpg123_read(&handle, buffer, sizeof(buffer), &nbytes);
		    error != MPG123_OK) {
			if (error != MPG123_DONE)
				FmtWarning(mpg123_domain,
					   "mpg123_read() failed: {}",
					   mpg123_plain_strerror(error));
			break;
		}

		/* update bitrate for ABR/VBR */
		if (info.vbr != MPG123_CBR) {
			/* FIXME: maybe skip, as too expensive? */
			/* FIXME: maybe, (info.vbr == MPG123_VBR) ? */
			if (mpg123_info(&handle, &info) != MPG123_OK)
				info.bitrate = 0;
		}

		/* send to MPD */

		cmd = client.SubmitAudio(is, std::span{buffer, nbytes},
					 info.bitrate);

		if (cmd == DecoderCommand::SEEK) {
			off_t c = client.GetSeekFrame();
			c = mpg123_seek(&handle, c, SEEK_SET);
			if (c < 0)
				client.SeekError();
			else {
				client.CommandFinished();
				client.SubmitTimestamp(audio_format.FramesToTime<FloatDuration>(c));
			}

			cmd = DecoderCommand::NONE;
		}
	} while (cmd == DecoderCommand::NONE);
}

static void
mpd_mpg123_stream_decode(DecoderClient &client, InputStream &is)
{
	/* open the file */

	int error;
	mpg123_handle *const handle = mpg123_new(nullptr, &error);
	if (handle == nullptr) {
		FmtError(mpg123_domain,
			 "mpg123_new() failed: {}",
			 mpg123_plain_strerror(error));
		return;
	}

	AtScopeExit(handle) { mpg123_delete(handle); };

	struct mpd_mpg123_iohandle iohandle{
		.client = &client,
		.is = is,
	};

	mpd_mpg123_open_stream(*handle, iohandle);

	if (is.KnownSize())
		mpg123_set_filesize(handle, is.GetSize());

	Decode(client, &is, *handle, is.IsSeekable());
}

static void
mpd_mpg123_file_decode(DecoderClient &client, Path path_fs)
{
	/* open the file */

	int error;
	mpg123_handle *const handle = mpg123_new(nullptr, &error);
	if (handle == nullptr) {
		FmtError(mpg123_domain,
			 "mpg123_new() failed: {}",
			 mpg123_plain_strerror(error));
		return;
	}

	AtScopeExit(handle) { mpg123_delete(handle); };

	if (!mpd_mpg123_open(handle, path_fs))
		return;

	Decode(client, nullptr, *handle, true);
}

static bool
Scan(mpg123_handle &handle, TagHandler &handler) noexcept
{
#ifdef ENABLE_ID3TAG
	/* prepare for using mpg123_id3_raw() later */
	mpg123_param(&handle, MPG123_ADD_FLAGS, MPG123_STORE_RAW_ID3, 0);
#endif

	AudioFormat audio_format;

	try {
		if (!GetAudioFormat(handle, audio_format))
			return false;
	} catch (...) {
		return false;
	}

	handler.OnAudioFormat(audio_format);

#ifdef ENABLE_ID3TAG
	if (const auto tag = ParseId3(handle))
		scan_id3_tag(tag.get(), handler);
#endif // ENABLE_ID3TAG

	if (const off_t num_samples = mpg123_length(&handle); num_samples >= 0) {
		const auto duration =
			SongTime::FromScale<uint64_t>(num_samples,
						      audio_format.sample_rate);
		handler.OnDuration(duration);
	}

	return true;
}

static bool
mpd_mpg123_scan_stream(InputStream &is, TagHandler &handler)
{
	int error;
	mpg123_handle *const handle = mpg123_new(nullptr, &error);
	if (handle == nullptr) {
		FmtError(mpg123_domain,
			 "mpg123_new() failed: {}",
			 mpg123_plain_strerror(error));
		return false;
	}

	AtScopeExit(handle) { mpg123_delete(handle); };

	struct mpd_mpg123_iohandle iohandle{
		.client = nullptr,
		.is = is,
	};

	mpd_mpg123_open_stream(*handle, iohandle);
	return Scan(*handle, handler);
}

static bool
mpd_mpg123_scan_file(Path path_fs, TagHandler &handler) noexcept
{
	int error;
	mpg123_handle *const handle = mpg123_new(nullptr, &error);
	if (handle == nullptr) {
		FmtError(mpg123_domain,
			 "mpg123_new() failed: {}",
			 mpg123_plain_strerror(error));
		return false;
	}

	AtScopeExit(handle) { mpg123_delete(handle); };

	try {
		if (!mpd_mpg123_open(handle, path_fs))
			return false;
	} catch (...) {
		return false;
	}

	return Scan(*handle, handler);
}

static const char *const mpg123_suffixes[] = {
	"mp3",
	nullptr
};

constexpr DecoderPlugin mpg123_decoder_plugin =
	DecoderPlugin("mpg123",
		      mpd_mpg123_stream_decode, mpd_mpg123_scan_stream,
		      mpd_mpg123_file_decode, mpd_mpg123_scan_file)
	.WithInit(mpd_mpg123_init, mpd_mpg123_finish)
	.WithSuffixes(mpg123_suffixes);
