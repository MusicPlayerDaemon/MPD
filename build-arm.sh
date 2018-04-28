#!/bin/sh

mBUILD=x86_64-unknown-linux-gnu
mHOST=arm-cortexa9_neon-linux-gnueabihf
mTARGET=arm-cortexa9_neon-linux-gnueabihf
mFPU=neon
mFLOAT_ABI=hard
mBUILD_TYPE=shared

mTOP_DIR=$PWD/..
mOUT_TOP=$mTOP_DIR/out/$mBUILD_TYPE-$mFPU
mPREFIX=$mOUT_TOP/_install

export PKG_CONFIG_PATH=$mPREFIX/lib/pkgconfig
export LDFLAGS=-L$mPREFIX/lib
export CPPFLAGS="-I$mPREFIX/include -I$mPREFIX/usr/include -mfpu=$mFPU -mfloat-abi=$mFLOAT_ABI -I$mPREFIX/system/include -I$mPREFIX/include/glib-2.0"

./autogen.sh

mkdir build/arm
cd build/arm

../../configure --prefix=$mPREFIX \
--build=$mBUILD  \
--host=$mHOST \
--target=$mTARGET \
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
--enable-fluidsynth=yes \
--enable-lsr=no \
--enable-opus=yes \
--enable-sidplay=no \
--enable-gme=no \
--enable-modplug=yes \
--with-zeroconf=avahi \
--enable-ao=no \
--enable-openal=no \
--disable-icu \
--disable-iconv \
--enable-tidal=no \
ac_cv_file__usr_local_include_rapidjson_rapidjson_h=yes \
$* \
LDFLAGS="-L$mPREFIX/lib -L$mPREFIX/usr/lib -L$mPREFIX/system/lib" \
LIBS="-liconv -Wl,-dynamic-linker=/system/bin/ld-linux-armhf.so.3" \
|| exit 0

make -j4 || exit 0
make -j4 install-strip || exit 0