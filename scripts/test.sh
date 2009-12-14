#!/bin/sh -e
#
# This shell script tests the build of MPD with various compile-time
# options.
#
# Author: Max Kellermann <max@duempel.org>

PREFIX=/tmp/mpd
rm -rf $PREFIX

test "x$MAKE" != x || MAKE=make

export CFLAGS="-Os"

test -x configure || NOCONFIGURE=1 ./autogen.sh

# all features on
./configure --prefix=$PREFIX/full \
    --disable-dependency-tracking --enable-debug --enable-werror \
    --enable-un \
    --enable-modplug \
    --enable-ao --enable-mikmod --enable-mvp
$MAKE install
$MAKE distclean

# no UN, no oggvorbis, no flac, enable oggflac
./configure --prefix=$PREFIX/small \
    --disable-dependency-tracking --enable-debug --enable-werror \
    --disable-un \
    --disable-flac --disable-vorbis --enable-oggflac
$MAKE install
$MAKE distclean

# strip down (disable TCP, disable nearly all plugins)
CFLAGS="$CFLAGS -DNDEBUG" \
./configure --prefix=$PREFIX/tiny \
    --disable-dependency-tracking --disable-debug --enable-werror \
    --disable-tcp \
    --disable-curl \
    --disable-id3 --disable-lsr \
    --disable-ao --disable-alsa --disable-jack --disable-pulse --disable-fifo \
    --disable-shout-ogg --disable-shout-mp3 --disable-lame-encoder \
    --disable-ffmpeg --disable-wavpack --disable-mpc --disable-aac \
    --disable-flac --disable-vorbis --disable-oggflac --disable-audiofile \
    --disable-cue \
    --with-zeroconf=no
$MAKE install
$MAKE distclean

# shout: ogg without mp3
# sndfile instead of modplug
./configure --prefix=$PREFIX/shout_ogg \
    --disable-dependency-tracking --disable-debug --enable-werror \
    --disable-tcp \
    --disable-curl \
    --disable-id3 --disable-lsr \
    --disable-ao --disable-alsa --disable-jack --disable-pulse --disable-fifo \
    --enable-shout-ogg --disable-shout-mp3 --disable-lame-encoder \
    --disable-ffmpeg --disable-wavpack --disable-mpc --disable-aac \
    --disable-flac --enable-vorbis --disable-oggflac --disable-audiofile \
    --disable-modplug --enable-sndfile \
    --with-zeroconf=no
$MAKE install
$MAKE distclean

# shout: mp3 without ogg
./configure --prefix=$PREFIX/shout_mp3 \
    --disable-dependency-tracking --disable-debug --enable-werror \
    --disable-tcp \
    --disable-curl \
    --disable-id3 --disable-lsr \
    --disable-ao --disable-alsa --disable-jack --disable-pulse --disable-fifo \
    --disable-shout-ogg --enable-shout-mp3 --enable-lame-encoder \
    --disable-ffmpeg --disable-wavpack --disable-mpc --disable-aac \
    --disable-flac --disable-vorbis --disable-oggflac --disable-audiofile \
    --with-zeroconf=no
$MAKE install
$MAKE distclean

# oggvorbis + oggflac
./configure --prefix=$PREFIX/oggvorbisflac \
    --disable-dependency-tracking --disable-debug --enable-werror \
    --disable-tcp \
    --disable-curl \
    --disable-id3 --disable-lsr \
    --disable-mp3 \
    --disable-ao --disable-alsa --disable-jack --disable-pulse --disable-fifo \
    --disable-shout-ogg --disable-shout-mp3 --disable-lame-encoder \
    --disable-ffmpeg --disable-wavpack --disable-mpc --disable-aac \
    --disable-flac --enable-vorbis --enable-oggflac --disable-audiofile \
    --with-zeroconf=no
$MAKE install
$MAKE distclean
