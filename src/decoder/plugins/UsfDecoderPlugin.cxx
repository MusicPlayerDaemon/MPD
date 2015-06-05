/*
 * Copyright (C) 2003-2015 The Music Player Daemon Project
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
#include "UsfDecoderPlugin.hxx"
#include "../DecoderAPI.hxx"
#include "../DecoderInternal.hxx"
#include "tag/TagHandler.hxx"
#include "fs/Path.hxx"
#include "util/Domain.hxx"
#include "Log.hxx"
#include "util/ScopeExit.hxx"

#include <usf.h>
#include <psflib.h>

static constexpr Domain usf_domain("usf");
static constexpr unsigned USF_BUFFER_FRAMES = 2048;
static constexpr unsigned USF_CHANNELS = 2;
static constexpr unsigned USF_BUFFER_SAMPLES = USF_BUFFER_FRAMES*USF_CHANNELS;

static void *
stdio_fopen( const char * path )
{
    return fopen( path, "rb" );
}

static size_t
stdio_fread( void *p, size_t size, size_t count, void *f )
{
    return fread( p, size, count, (FILE*) f );
}

static int
stdio_fseek( void * f, int64_t offset, int whence )
{
    return fseek( (FILE*) f, offset, whence );
}

static int
stdio_fclose( void * f )
{
    return fclose( (FILE*) f );
}

static long
stdio_ftell( void * f )
{
    return ftell( (FILE*) f );
}

static constexpr psf_file_callbacks stdio_callbacks =
{
    "\\/:",
    stdio_fopen,
    stdio_fread,
    stdio_fseek,
    stdio_fclose,
    stdio_ftell
};

struct UsfLoaderState
{
    bool enableCompare; // The _enablecompare tag is present in the file; passed to usf_set_compare
    bool enableFIFOFull; // The _enableFIFOfull tag is present in the file; passed to usf_set_fifo_full
    void * emu; // The emulator state
};

static int
usf_loader(void * context, const uint8_t * exe, size_t exe_size,
               const uint8_t * reserved, size_t reserved_size)
{
    struct UsfLoaderState * state = ( struct UsfLoaderState * ) context;

    if ( exe && exe_size > 0 ) return -1;

    return usf_upload_section( state->emu, reserved, reserved_size );
}

static int
usf_info(void * context, const char * name, const char * value)
{
    struct UsfLoaderState * state = ( struct UsfLoaderState * ) context;
    if (!value) return 0;

    if ( strcmp( name, "_enablecompare" ) == 0)
        state->enableCompare = 1;
    else if ( strcmp( name, "_enableFIFOfull" ) == 0)
        state->enableFIFOFull = 1;

    return 0;
}

struct UsfTags {
    double length = -1; // Track duration. -1 represents looping infinitely
    double fade = 0;   // Fade out duration
    bool setTags = false;  // If set, set the tags with the below variables
    const TagHandler * tagHandler;
    void * handler_ctx;
};

static double
get_length_from_string(const char * string)
{
    size_t len = strlen(string);
    int32_t total = 0; // Total time in milliseconds
    int32_t finalMult = 1000; // Multiplier for the final unit. If no delimiter is used, use seconds.
    int32_t localMult = 1; // Multiplier in the currently parsed unit (millisecond, second, minute).
    int32_t tmp = 0; // 
    for (int i = len - 1; i >= 0; i--) {
        char c = string[i];
        if (c >= '0' && c <= '9') {
            tmp += (c - '0') * localMult;
            localMult *= 10;
        } else {
            localMult = 1;
            int mult = 0;
            if (c == '.') {
                mult = 1;
            } else if (c == ':') {
                mult = 1000;
                finalMult = 60000;
            } else {
                return -1; // Error in parsing duration. Return -1 (looping).
            }
            total += tmp * mult;
            tmp = 0;
        }
    }
    total += finalMult*tmp;

    return total/1000;
}

static bool
usf_set_tag(const char * field, const char * name, const char *value, TagType type, UsfTags * tags)
{
    if (strcmp(name, field) == 0) {
        tag_handler_invoke_tag(*tags->tagHandler, tags->handler_ctx, type, value);
        return true;
    }
    return false;
}

static int
usf_tags_target(void * context, const char * name, const char * value)
{
    struct UsfTags * tags = ( struct UsfTags * ) context;
    if (!value) return 0;

    if (tags->setTags) {
        usf_set_tag("title", name, value, TAG_TITLE, tags) ||
            usf_set_tag("artist", name, value, TAG_ARTIST, tags) ||
            usf_set_tag("composer", name, value, TAG_COMPOSER, tags) ||
            usf_set_tag("game", name, value, TAG_ALBUM, tags) ||
            usf_set_tag("year", name, value, TAG_DATE, tags) ||
            usf_set_tag("genre", name, value, TAG_GENRE, tags) ||
            usf_set_tag("track", name, value, TAG_TRACK, tags);
    }

    if (strcmp("length", name) == 0) {
        tags->length = get_length_from_string(value);
    } else if (strcmp("fade", name) == 0) {
        tags->fade = get_length_from_string(value);
    }
    return 0;
}


inline static double
track_lengths(double length, double fade)
{
    return length + fade;

}

static void
usf_file_decode(Decoder &decoder, Path path_fs)
{
    /* Load the file */

    UsfTags tags;
    UsfLoaderState state;
    state.emu = malloc(usf_get_state_size());
    usf_clear(state.emu);
    AtScopeExit(state) {
        free(state.emu);
    };

    const char* path = path_fs.c_str();
    const int psf_version_tags = psf_load(path, &stdio_callbacks, 0, 0, 0, usf_tags_target, &tags, 0);
    const int psf_version_state = psf_load( path, &stdio_callbacks, psf_version_tags, usf_loader, &state, usf_info, &state, 0 );

    if ( psf_version_tags != 0x21 || psf_version_state <= 0 ) {
        LogWarning(usf_domain, "Error loading usf file");
        return;
    }

    usf_set_compare( state.emu, state.enableCompare );
    usf_set_fifo_full( state.emu, state.enableFIFOFull );

    int32_t sample_rate;
    usf_render( state.emu, 0, 0, &sample_rate );

    /* initialize the MPD decoder */

    const AudioFormat audio_format(sample_rate, SampleFormat::S16, USF_CHANNELS);
    assert(audio_format.IsValid());

    double totalLength =  track_lengths(tags.length, tags.fade);

    // Duration
    decoder_initialized(decoder, audio_format, true, SongTime::FromS(totalLength));

    /* .. and play */
    DecoderCommand cmd;
    do {
        int16_t buf[USF_BUFFER_SAMPLES];
        const char* result = usf_render(state.emu, buf, USF_BUFFER_FRAMES, 0);
        if (result != 0) {
            LogWarning(usf_domain, result);
            break;
        }

        // Simple fading
        double fadeTime = tags.fade;
        double trackLength = tags.length;
        if (fadeTime > 0 && totalLength >= 0 && decoder.timestamp > trackLength) {
            const double vol = 1.0 - ((decoder.timestamp - trackLength) / fadeTime);
            const double normalizedVol = vol < 0 ? 0 : vol;
            for (unsigned int i = 0; i < USF_BUFFER_SAMPLES; i++) {
                buf[i] *= normalizedVol;
            }
        }

        cmd = decoder_data(decoder, nullptr, buf, sizeof(buf), 0);

        // Stop song manually
        if (totalLength >= 0 && decoder.timestamp > totalLength+2)
            break;

        if (cmd == DecoderCommand::SEEK) {
            // If user seeks during the fade period. Disable fading and play forever.
            // Hacky way to give user posibility to enable looping on the fly
            if (decoder.timestamp > trackLength) {
                totalLength = -1;
            }
            // Seek manually by restarting emulator and discarding samples.
            const double target_time = decoder_seek_time(decoder).ToDoubleS();
            usf_restart(state.emu);
            const unsigned frames_to_throw = (unsigned) (sample_rate*target_time+0.5);
            usf_render(state.emu, 0, frames_to_throw, 0);

            // Time correction after seek. Decided by trial and error.
            decoder_command_finished(decoder);
            double newTime = (frames_to_throw/(double)sample_rate)-0.5;
            decoder_timestamp(decoder, newTime);
        }

    } while (cmd != DecoderCommand::STOP);
    usf_shutdown(state.emu);
}

static bool
usf_scan_file(Path path_fs, const struct TagHandler &handler, void *handler_ctx) 
{
    const char* path = path_fs.c_str();
    UsfTags tags;
    tags.setTags = true;
    tags.tagHandler = &handler;
    tags.handler_ctx = handler_ctx;
    const int psf_version = psf_load(path, &stdio_callbacks, 0, 0, 0, usf_tags_target, &tags, 0);
    if (psf_version < 0) {
        return false;
    }

    // Duration
    double totalLength = track_lengths(tags.length, tags.fade);

    tag_handler_invoke_duration(handler, handler_ctx, SongTime::FromS(totalLength));
    return true;
}

static const char *const usf_suffixes[] = {
	"usf",
	"miniusf",
	nullptr
};

extern const struct DecoderPlugin usf_decoder_plugin;
const struct DecoderPlugin usf_decoder_plugin = {
	"usf",
	nullptr,
	nullptr,
	nullptr, /* stream_decode() */
	usf_file_decode,
	usf_scan_file,
	nullptr, /* stream_tag() */
	nullptr,
	usf_suffixes,
	nullptr, /* mime_types */
};
