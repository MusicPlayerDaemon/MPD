#!/bin/sh

./autogen.sh

mkdir build/native
cd build/native

../../configure \
--enable-syslog=no \
--sysconfdir=/etc \
--enable-documentation=no \
--enable-smbclient=yes \
--enable-upnp=yes \
--disable-nfs \
--enable-iso9660=no \
--enable-zlib=yes \
--enable-zzip=no \
--enable-systemd-daemon=no \
--enable-cdio-paranoia=no \
--enable-curl=yes \
--enable-roar=no \
--enable-jack=no \
--enable-pulse=no \
--enable-sidplay=no \
--enable-solaris-output=no \
--enable-shout=no \
--enable-soundcloud=no \
--enable-audiofile=no \
--enable-sndfile=yes \
--enable-fluidsynth=no \
--enable-lsr=no \
--enable-opus=yes \
--enable-sidplay=no \
--enable-gme=no \
--enable-modplug=no \
--with-zeroconf=avahi \
--enable-ao=no \
--enable-openal=no \
--disable-icu \
--disable-iconv \
--enable-tidal=yes \
--enable-qobuz=yes \
ac_cv_file__usr_local_include_rapidjson_rapidjson_h=yes \
$* \
|| exit 0

make -j4 || exit 0
sudo make -j4 install-strip || exit 0