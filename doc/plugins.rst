Plugin reference
################

.. _database_plugins:

Database plugins
================

simple
------

The default plugin. Stores a copy of the database in memory. A file is used for permanent storage.

.. list-table::
   :widths: 20 80                     
   :header-rows: 1

   * - Setting
     - Description
   * - **path**
     - The path of the database file. 
   * - **cache_directory**
     - The path of the cache directory for additional storages mounted at runtime. This setting is necessary for the **mount** protocol command.
   * - **compress yes|no**
     - Compress the database file using gzip? Enabled by default (if built with zlib).
   * - **hide_playlist_targets yes|no**
     - Hide songs which are referenced by playlists?  Thas is,
       playlist files which are represented in the database as virtual
       directories (playlist plugin setting ``as_directory``).  This
       option is enabled by default and avoids duplicate songs; one
       copy for the original file, and another copy in the virtual
       directory of a CUE file referring to it.

proxy
-----

Provides access to the database of another :program:`MPD` instance
using `libmpdclient
<https://www.musicpd.org/libs/libmpdclient/>`_. This is useful when
you mount the music directory via NFS/SMB, and the file server already
runs a :program:`MPD` (0.20 or newer) instance. Only the file server
needs to update the database.

.. list-table::
   :widths: 20 80                     
   :header-rows: 1

   * - Setting
     - Description
   * - **host**
     - The host name of the "master" :program:`MPD` instance.
   * - **port**
     - The port number of the "master" :program:`MPD` instance.
   * - **password**
     - The password used to log in to the "master" :program:`MPD` instance.
   * - **keepalive yes|no**
     - Send TCP keepalive packets to the "master" :program:`MPD` instance? This option can help avoid certain firewalls dropping inactive connections, at the expense of a very small amount of additional network traffic. Disabled by default.

upnp
----

Provides access to UPnP media servers.

.. list-table::
   :widths: 20 80
   :header-rows: 1

   * - Setting
     - Description
   * - **interface**
     - Interface used to discover media servers. Decided by upnp if left unconfigured.

Storage plugins
===============

local
-----

The default plugin which gives :program:`MPD` access to local files. It is used when music_directory refers to a local directory.

curl
----

A WebDAV client using libcurl. It is used when :code:`music_directory`
contains a ``http://`` or ``https://`` URI, for example
:samp:`https://the.server/dav/`.

smbclient
---------

Load music files from a SMB/CIFS server. It is used when
:code:`music_directory` contains a ``smb://`` URI, for example
:samp:`smb://myfileserver/Music`.

Note that :file:`libsmbclient` has a serious bug which causes MPD to
crash, and therefore this plugin is disabled by default and should not
be used until the bug is fixed:
https://bugzilla.samba.org/show_bug.cgi?id=11413

nfs
---

Load music files from a NFS server.  It is used when
:code:`music_directory` contains a ``nfs://`` URI according to
RFC2224, for example :samp:`nfs://servername/path`.

See :ref:`input_nfs` for more information.

udisks
------

Mount file systems (e.g. USB sticks or other removable media) using
the udisks2 daemon via D-Bus.  To obtain a valid udisks2 URI, consult
:ref:`the according neighbor plugin <neighbor_plugin>`.

It might be necessary to grant :program:`MPD` privileges to control
:program:`udisks2` through :program:`policykit`.  To do this, create a
file called :file:`/usr/share/polkit-1/rules.d/mpd-udisks.rules` with
the following text::

 polkit.addRule(function(action, subject) {
   if ((action.id == "org.freedesktop.udisks2.filesystem-mount" ||
        action.id == "org.freedesktop.udisks2.filesystem-mount-other-seat") &&
       subject.user == "mpd") {
       return polkit.Result.YES;
   }
 });

If you run MPD as a different user, change ``mpd`` to the name of your
MPD user.

.. _neighbor_plugin:

Neighbor plugins
================

smbclient
---------

Provides a list of SMB/CIFS servers on the local network.

udisks
------

Queries the udisks2 daemon via D-Bus and obtains a list of file systems (e.g. USB sticks or other removable media).

upnp
----

Provides a list of UPnP servers on the local network.

.. _input_plugins:

Input plugins
=============

alsa
----

Allows :program:`MPD` on Linux to play audio directly from a soundcard using the scheme alsa://. Audio is by default formatted as 48 kHz 16-bit stereo, but this default can be overidden by a config file setting or by the URI. Examples:

.. code-block:: none

    mpc add alsa:// plays audio from device default

.. code-block:: none

    mpc add alsa://hw:1,0 plays audio from device hw:1,0

.. code-block:: none

    mpc add alsa://hw:1,0?format=44100:16:2 plays audio from device hw:1,0 sampling 16-bit stereo at 44.1kHz.

.. list-table::
   :widths: 20 80
   :header-rows: 1

   * - Setting
     - Description
   * - **default_device NAME**
     - The alsa device id to use when none is specified in the URI.
   * - **default_format F**
     - The sampling rate, size and channels to use. Wildcards are not allowed.

       Example - "44100:16:2"

   * - **auto_resample yes|no**
     - If set to no, then libasound will not attempt to resample. In this case, the user is responsible for ensuring that the requested sample rate can be produced natively by the device, otherwise an error will occur.
   * - **auto_channels yes|no**
     - If set to no, then libasound will not attempt to convert between different channel numbers. The user must ensure that the device supports the requested channels when sampling.
   * - **auto_format yes|no**
     - If set to no, then libasound will not attempt to convert between different sample formats (16 bit, 24 bit, floating point, ...). Again the user must ensure that the requested format is available natively from the device.

cdio_paranoia
-------------

Plays audio CDs using libcdio. The URI has the form: "cdda://[DEVICE][/TRACK]". The simplest form cdda:// plays the whole disc in the default drive.

.. list-table::
   :widths: 20 80
   :header-rows: 1

   * - Setting
     - Description
   * - **default_byte_order little_endian|big_endian**
     - If the CD drive does not specify a byte order, MPD assumes it is the CPU's native byte order. This setting allows overriding this.
   * - **speed N**
     - Request CDParanoia cap the extraction speed to Nx normal CD audio rotation speed, keeping the drive quiet.

curl
----

Opens remote files or streams over HTTP using libcurl.

Note that unless overridden by the below settings (e.g. by setting
them to a blank value), general curl configuration from environment
variables such as ``http_proxy`` or specified in :file:`~/.curlrc`
will be in effect.

.. list-table::
   :widths: 20 80
   :header-rows: 1

   * - Setting
     - Description
   * - **proxy**
     - Sets the address of the HTTP proxy server.
   * - **proxy_user, proxy_password**
     - Configures proxy authentication.
   * - **verify_peer yes|no**
     - Verify the peer's SSL certificate? `More information <http://curl.haxx.se/libcurl/c/CURLOPT_SSL_VERIFYPEER.html>`_.
   * - **verify_host yes|no**
     - Verify the certificate's name against host? `More information <http://curl.haxx.se/libcurl/c/CURLOPT_SSL_VERIFYHOST.html>`_.
   * - **cacert**
     - Set path to Certificate Authority (CA) bundle `More information <https://curl.se/libcurl/c/CURLOPT_CAINFO.html>`_.

ffmpeg
------

Access to various network protocols implemented by the FFmpeg library:
``gopher://``, ``rtp://``, ``rtsp://``, ``rtmp://``, ``rtmpt://``,
``rtmps://``

file
----

Opens local files

mms
---

Plays streams with the MMS protocol using `libmms <https://launchpad.net/libmms>`_.

.. _input_nfs:

nfs
---

Allows :program:`MPD` to access files on NFS servers without actually
mounting them (i.e. with :program:`libnfs` in userspace, without help
from the kernel's VFS layer). All URIs with the ``nfs://`` scheme are
used according to RFC2224. Example:

.. code-block:: none

     mpc add nfs://servername/path/filename.ogg

This plugin uses :program:`libnfs`, which supports only NFS version 3.
Since :program:`MPD` is not allowed to bind to so-called "privileged
ports", the NFS server needs to enable the ``insecure`` setting;
example :file:`/etc/exports`:

.. code-block:: none

    /srv/mp3 192.168.1.55(ro,insecure)

Don't fear: this will not make your file server insecure; the flag was
named a time long ago when privileged ports were thought to be
meaningful for security. By today's standards, NFSv3 is not secure at
all, and if you believe it is, you're already doomed.

smbclient
---------

Allows :program:`MPD` to access files on SMB/CIFS servers (e.g. Samba
or Microsoft Windows). All URIs with the ``smb://`` scheme are
used.  Example:

.. code-block:: none

    mpc add smb://servername/sharename/filename.ogg
    mpc add smb://username:password@servername/sharename/filename.ogg

qobuz
-----

Play songs from the commercial streaming service Qobuz. It plays URLs
in the form ``qobuz://track/ID``, e.g.:

.. code-block:: none

    mpc add qobuz://track/23601296

.. list-table::
   :widths: 20 80
   :header-rows: 1

   * - Setting
     - Description
   * - **app_id ID**
     - The Qobuz application id.
   * - **app_secret SECRET**
     - The Qobuz application secret.
   * - **username USERNAME**
     - The Qobuz user name.
   * - **password PASSWORD**
     - The Qobuz password.
   * - **format_id N**
     - The `Qobuz format identifier <https://github.com/Qobuz/api-documentation/blob/master/endpoints/track/getFileUrl.md#parameters>`_, i.e. a number which chooses the format and quality to be requested from Qobuz. The default is "5" (320 kbit/s MP3).

.. _decoder_plugins:
     
Decoder plugins
===============

adplug
------

Decodes AdLib files using libadplug.

.. list-table::
   :widths: 20 80
   :header-rows: 1

   * - Setting
     - Description
   * - **sample_rate**
     - The sample rate that shall be synthesized by the plugin. Defaults to 48000.

audiofile
---------

Decodes WAV and AIFF files using libaudiofile.

faad
----

Decodes AAC files using libfaad.

.. _decoder_ffmpeg:

ffmpeg
------

Decodes various codecs using FFmpeg.

.. list-table::
   :widths: 20 80
   :header-rows: 1

   * - Setting
     - Description
   * - **analyzeduration VALUE**
     - Sets the FFmpeg muxer option analyzeduration, which specifies how many microseconds are analyzed to probe the input. The `FFmpeg formats documentation <https://ffmpeg.org/ffmpeg-formats.html>`_ has more information.
   * - **probesize VALUE**
     - Sets the FFmpeg muxer option probesize, which specifies probing size in bytes, i.e. the size of the data to analyze to get stream information. The `FFmpeg formats documentation <https://ffmpeg.org/ffmpeg-formats.html>`_ has more information.

flac
----

Decodes FLAC files using libFLAC.

.. _decoder_dsdiff:

dsdiff
------

Decodes DSDIFF (`Direct Stream Digital Interchange File Format
<http://www.sonicstudio.com/pdf/dsd/DSDIFF_1.5_Spec.pdf>`_) files
(:file:`*.dff`).  These contain :ref:`DSD <dsd>` instead of PCM.

.. list-table::
   :widths: 20 80
   :header-rows: 1

   * - Setting
     - Description
   * - **lsbitfirst yes|no**
     - Decode the least significant bit first. Default is no.

.. _decoder_dsf:

dsf
---

Decodes DSF
(<https://dsd-guide.com/sites/default/files/white-papers/DSFFileFormatSpec_E.pdf>)
files (:file:`*.dsf`).  These contain :ref:`DSD <dsd>` instead of PCM.

fluidsynth
----------

MIDI decoder based on `FluidSynth <http://www.fluidsynth.org/>`_.

.. list-table::
   :widths: 20 80
   :header-rows: 1

   * - Setting
     - Description
   * - **sample_rate**
     - The sample rate that shall be synthesized by the plugin. Defaults to 48000.
   * - **soundfont**
     - The absolute path of the soundfont file. Defaults to :file:`/usr/share/sounds/sf2/FluidR3_GM.sf2`.

gme
---

Video game music file emulator based on `game-music-emu <https://bitbucket.org/mpyne/game-music-emu/wiki/Home>`_.

.. list-table::
   :widths: 20 80
   :header-rows: 1

   * - Setting
     - Description
   * - **accuracy yes|no**
     - Enable more accurate sound emulation.
   * - **default_fade**
     - The default fade-out time, in seconds. Used by songs that don't specify their own fade-out time.

hybrid_dsd
----------

`Hybrid-DSD
<http://dsdmaster.blogspot.de/p/bitperfect-introduces-hybrid-dsd-file.html>`_
is an MP4 container file (:file:`*.m4a`) which contains both ALAC and
DSD data. It is disabled by default, and works only if you explicitly
enable it. Without this plugin, the ALAC parts gets handled by the
:ref:`FFmpeg decoder plugin <decoder_ffmpeg>`. This
plugin should be enabled only if you have a bit-perfect playback path
to a DSD-capable DAC; for everybody else, playing back the ALAC copy
of the file is better.

mad
---

Decodes MP3 files using `libmad <http://www.underbit.com/products/mad/>`_.

mikmod
------

Module player based on `MikMod <http://mikmod.sourceforge.net/>`_.

.. list-table::
   :widths: 20 80
   :header-rows: 1

   * - Setting
     - Description
   * - **loop yes|no**
     - Allow backward loops in modules. Default is no.
   * - **sample_rate**
     - Sets the sample rate generated by libmikmod. Default is 44100.

modplug
-------

Module player based on MODPlug.

.. list-table::
   :widths: 20 80
   :header-rows: 1

   * - Setting
     - Description
   * - **resampling_mode nearest|linear|spline|fir**
     - Sets the resampling mode. "nearest" disables interpolation (good for chiptunes). "linear" makes modplug use linear interpolation (fast, good quality). "spline" makes modplug use cubic spline interpolation (high quality). "fir" makes modplug use 8-tap fir filter (extremely high quality). Defaults to "fir".
   * - **loop_count**
     - Number of times to loop the module if it uses backward loops. Default is 0 which prevents looping. -1 loops forever.

openmpt
-------

Module player based on `libopenmpt <https://lib.openmpt.org>`_.

.. list-table::
   :widths: 20 80
   :header-rows: 1

   * - Setting
     - Description
   * - **repeat_count**
     - Set how many times the module repeats. -1: repeat forever. 0: play once, repeat zero times (the default). n>0: play once and repeat n times after that.
   * - **stereo_separation**
     - Sets the stereo separation. The supported value range is [0,200]. Defaults to 100.
   * - **interpolation_filter 0|1|2|4|8**
     - Sets the interpolation filter. 0: internal default. 1: no interpolation (zero order hold). 2: linear interpolation. 4: cubic interpolation. 8: windowed sinc with 8 taps. Defaults to 0.
   * - **override_mptm_interp_filter yes|no**
     - If `interpolation_filter` has been changed, setting this to yes will force all MPTM modules to use that interpolation filter. If set to no, MPTM modules will play with their own interpolation filter regardless of the value of `interpolation_filter`. Defaults to no.
   * - **volume_ramping**
     - Sets the amount of volume ramping done by the libopenmpt mixer. The default value is -1, which indicates a recommended default value. The meaningful value range is [-1..10]. A value of 0 completely disables volume ramping. This might cause clicks in sound output. Higher values imply slower/softer volume ramps.
   * - **sync_samples yes|no**
     - Syncs sample playback when seeking. Defaults to yes.
   * - **emulate_amiga yes|no**
     - Enables the Amiga resampler for Amiga modules. This emulates the sound characteristics of the Paula chip and overrides the selected interpolation filter. Non-Amiga module formats are not affected by this setting. Defaults to yes.
   * - **emulate_amiga_type**
     - Configures the filter type to use for the Amiga resampler. Supported values are: "auto": Filter type is chosen by the library and might change. This is the default. "a500": Amiga A500 filter. "a1200": Amiga A1200 filter. "unfiltered": BLEP synthesis without model-specific filters. The LED filter is ignored by this setting. This filter mode is considered to be experimental and might change in the future. Defaults to "auto". Requires libopenmpt 0.5 or higher.

mpcdec
------

Decodes Musepack files using `libmpcdec <http://www.musepack.net/>`_.

mpg123
------

Decodes MP3 files using `libmpg123 <http://www.mpg123.de/>`_. Currently, this
decoder does not support streams (e.g. archived files, remote files over HTTP,
...), only regular local files.

opus
----

Decodes Opus files using `libopus <http://www.opus-codec.org/>`_.

pcm
---

Reads raw PCM samples. It understands the "audio/L16" MIME type with parameters "rate" and "channels" according to RFC 2586. It also understands the MPD-specific MIME type "audio/x-mpd-float".

sidplay
-------

C64 SID decoder based on `libsidplayfp <https://sourceforge.net/projects/sidplay-residfp/>`_ or `libsidplay2 <https://sourceforge.net/projects/sidplay2/>`_.

.. list-table::
   :widths: 20 80
   :header-rows: 1

   * - Setting
     - Description
   * - **songlength_database PATH**
     - Location of your songlengths file, as distributed with the HVSC. The sidplay plugin checks this for matching MD5 fingerprints. See http://www.hvsc.c64.org/download/C64Music/DOCUMENTS/Songlengths.faq. New songlength format support requires libsidplayfp 2.0 or later.
   * - **default_songlength SECONDS**
     - This is the default playing time in seconds for songs not in the songlength database, or in case you're not using a database. A value of 0 means play indefinitely.
   * - **default_genre GENRE**
     - Optional default genre for SID songs.
   * - **filter yes|no**
     - Turns the SID filter emulation on or off.
   * - **kernal**
     - Only libsidplayfp. Roms are not embedded in libsidplayfp - please note https://sourceforge.net/p/sidplay-residfp/news/2013/01/released-libsidplayfp-100beta1/ But some SID tunes require rom images to play. Make C64 rom dumps from your own vintage gear or use rom files from Frodo or VICE emulation software tarballs. Absolute path to kernal rom image file.
   * - **basic**
     - Only libsidplayfp. Absolute path to basic rom image file.

sndfile
-------

Decodes WAV and AIFF files using `libsndfile <http://www.mega-nerd.com/libsndfile/>`_.


vorbis
------

Decodes Ogg-Vorbis files using `libvorbis <http://www.xiph.org/ogg/vorbis/>`_.

wavpack
-------

Decodes WavPack files using `libwavpack <http://www.wavpack.com/>`_.

wildmidi
--------

MIDI decoder based on `libwildmidi <http://www.mindwerks.net/projects/wildmidi/>`_.

.. list-table::
   :widths: 20 80
   :header-rows: 1

   * - Setting
     - Description
   * - **config_file**
     - The absolute path of the timidity config file. Defaults to :file:`/etc/timidity/timidity.cfg`.

.. _encoder_plugins:
     
Encoder plugins
===============

flac
----

Encodes into `FLAC <https://xiph.org/flac/>`_ (lossless).

.. list-table::
   :widths: 20 80
   :header-rows: 1

   * - Setting
     - Description
   * - **compression**
     - Sets the libFLAC compression level. The levels range from 0 (fastest, least compression) to 8 (slowest, most compression).
   * - **oggflac yes|no**
     - Configures if the stream should be Ogg FLAC versus native FLAC. Defaults to "no" (use native FLAC).
   * - **oggchaining yes|no**
     - Configures if the stream should use Ogg Chaining for in-stream metadata. Defaults to "no". Setting this to "yes" also enables Ogg FLAC.

lame
----

Encodes into MP3 using the `LAME <http://lame.sourceforge.net/>`_ library.

.. list-table::
   :widths: 20 80
   :header-rows: 1

   * - Setting
     - Description
   * - **quality**
     - Sets the quality for VBR. 0 is the highest quality, 9 is the lowest quality. Cannot be used with bitrate.
   * - **bitrate**
     - Sets the bit rate in kilobit per second. Cannot be used with quality.

null
----

Does not encode anything, passes the input PCM data as-is.

shine
-----

Encodes into MP3 using the `Shine <https://github.com/savonet/shine>`_ library.

.. list-table::
   :widths: 20 80
   :header-rows: 1

   * - Setting
     - Description
   * - **bitrate**
     - Sets the bit rate in kilobit per second.

twolame
-------

Encodes into MP2 using the `TwoLAME <http://www.twolame.org/>`_ library.

.. list-table::
   :widths: 20 80
   :header-rows: 1

   * - Setting
     - Description
   * - **quality**
     - Sets the quality for VBR. 0 is the highest quality, 9 is the lowest quality. Cannot be used with bitrate.
   * - **bitrate**
     - Sets the bit rate in kilobit per second. Cannot be used with quality.

opus
----

Encodes into `Ogg Opus <http://www.opus-codec.org/>`_.

.. list-table::
   :widths: 20 80
   :header-rows: 1

   * - Setting
     - Description
   * - **bitrate**
     - Sets the data rate in bits per second. The special value "auto" lets libopus choose a rate (which is the default), and "max" uses the maximum possible data rate.
   * - **complexity**
     - Sets the `Opus complexity <https://wiki.xiph.org/OpusFAQ#What_is_the_complexity_of_Opus.3F>`_.
   * - **signal**
     - Sets the Opus signal type. Valid values are "auto" (the default), "voice" and "music".
   * - **vbr yes|no|constrained**
     - Sets the vbr mode. Setting to "yes" (default) enables variable bitrate, "no" forces constant bitrate and frame sizes, "constrained" uses constant bitrate analogous to CBR in AAC and MP3.
   * - **packet_loss**
     - Sets the expected packet loss percentage. This value can be increased from the default "0" for a more redundant stream at the expense of quality.
   * - **opustags yes|no**
     - Configures how metadata is interleaved into the stream. If set to yes, then metadata is inserted using ogg stream chaining, as specified in :rfc:`7845`. If set to no (the default), then ogg stream chaining is avoided and other output-dependent method is used, if available.

.. _vorbis_plugin:

vorbis
------

Encodes into `Ogg Vorbis <http://www.vorbis.com/>`_.

.. list-table::
   :widths: 20 80
   :header-rows: 1

   * - Setting
     - Description
   * - **quality**
     - Sets the quality for VBR. -1 is the lowest quality, 10 is the highest quality. Defaults to 3. Cannot be used with bitrate.
   * - **bitrate**
     - Sets the bit rate in kilobit per second. Cannot be used with quality.

wave
----
Encodes into WAV (lossless).

.. _resampler_plugins:

Resampler plugins
=================

The resampler can be configured in a block named resampler, for example:

.. code-block:: none

    resampler {
      plugin "soxr"
      quality "very high"
    }

The following table lists the resampler options valid for all plugins:

.. list-table::
   :widths: 20 80
   :header-rows: 1

   * - Name
     - Description
   * - **plugin**
     - The name of the plugin.

internal
--------

A resampler built into :program:`MPD`. Its quality is very poor, but its CPU usage is low. This is the fallback if :program:`MPD` was compiled without an external resampler.

libsamplerate
-------------

A resampler using `libsamplerate <http://www.mega-nerd.com/SRC/>`_ a.k.a. Secret Rabbit Code (SRC).

.. list-table::
   :widths: 20 80
   :header-rows: 1

   * - Name
     - Description
   * - **type**
     - The interpolator type. Defaults to :samp:`2`. See below for a list of known types.

The following converter types are provided by libsamplerate:

.. list-table::
   :widths: 20 80
   :header-rows: 1

   * - Type
     - Description
   * - **"Best Sinc Interpolator" or "0"**
     - Band limited sinc interpolation, best quality, 97dB SNR, 96% BW.
   * - **"Medium Sinc Interpolator" or "1"**
     - Band limited sinc interpolation, medium quality, 97dB SNR, 90% BW.
   * - **"Fastest Sinc Interpolator" or "2"**
     - Band limited sinc interpolation, fastest, 97dB SNR, 80% BW.
   * - **"ZOH Sinc Interpolator" or "3"**
     - Zero order hold interpolator, very fast, very poor quality with audible distortions.
   * - **"Linear Interpolator" or "4"**
     - Linear interpolator, very fast, poor quality.

soxr
----

A resampler using `libsoxr <http://sourceforge.net/projects/soxr/>`_, the SoX Resampler library

.. list-table::
   :widths: 20 80
   :header-rows: 1

   * - Name
     - Description
   * - **quality**
     - The libsoxr quality setting. Valid values see below.
   * - **threads**
     - The number of libsoxr threads. "0" means "automatic". The default is "1" which disables multi-threading.

Valid quality values for libsoxr:

* "very high"
* "high" (the default)
* "medium"
* "low"
* "quick"
* "custom"

If the quality is set to custom also the following settings are available:

.. list-table::
   :widths: 20 80
   :header-rows: 1

   * - Name
     - Description
   * - **precision**
     - The precision in bits. Valid values 16,20,24,28 and 32  bits.
   * - **phase_response**
     - Between the 0-100, Where 0=MINIMUM_PHASE and 50=LINEAR_PHASE.
   * - **passband_end**
     - The % of source bandwidth where to start filtering. Typical between the 90-99.7.
   * - **stopband_begin**
     - The % of the source bandwidth Where the anti aliasing filter start. Value 100+.
   * - **attenuation**
     - Reduction in dB's to prevent clipping from the resampling process.
   * - **flags**
     - Bitmask with additional option see soxr documentation for specific flags.


.. _output_plugins:

Output plugins
==============

.. _alsa_plugin:

alsa
----

The `Advanced Linux Sound Architecture (ALSA) <http://www.alsa-project.org/>`_ plugin uses libasound. It is recommended if you are using Linux.

.. list-table::
   :widths: 20 80
   :header-rows: 1

   * - Setting
     - Description
   * - **device NAME**
     - Sets the device which should be used. This can be any valid ALSA device name. The default value is "default", which makes libasound choose a device. It is recommended to use a "hw" or "plughw" device, because otherwise, libasound automatically enables "dmix", which has major disadvantages (fixed sample rate, poor resampler, ...).
   * - **buffer_time US**
     - Sets the device's buffer time in microseconds. Don't change unless you know what you're doing.
   * - **period_time US**
     - Sets the device's period time in microseconds. Don't change unless you really know what you're doing.
   * - **auto_resample yes|no**
     - If set to no, then libasound will not attempt to resample, handing the responsibility over to MPD. It is recommended to let MPD resample (with libsamplerate), because ALSA is quite poor at doing so.
   * - **auto_channels yes|no**
     - If set to no, then libasound will not attempt to convert between different channel numbers.
   * - **auto_format yes|no**
     - If set to no, then libasound will not attempt to convert between different sample formats (16 bit, 24 bit, floating point, ...).
   * - **dop yes|no**
     - If set to yes, then DSD over PCM according to the `DoP standard <http://dsd-guide.com/dop-open-standard>`_ is enabled. This wraps DSD samples in fake 24 bit PCM, and is understood by some DSD capable products, but may be harmful to other hardware. Therefore, the default is no and you can enable the option at your own risk.
   * - **stop_dsd_silence yes|no**
     - If enabled, silence is played before manually stopping playback
       ("stop" or "pause") in DSD mode (native DSD or DoP).  This is a
       workaround for some DACs which emit noise when stopping DSD
       playback.
   * - **thesycon_dsd_workaround yes|no**
     - If enabled, enables a workaround for a bug in Thesycon USB
       audio receivers.  On these devices, playing DSD512 or PCM
       causes all subsequent attempts to play other DSD rates to fail,
       which can be fixed by briefly playing PCM at 44.1 kHz.
   * - **allowed_formats F1 F2 ...**
     - Specifies a list of allowed audio formats, separated by a space. All items may contain asterisks as a wild card, and may be followed by "=dop" to enable DoP (DSD over PCM) for this particular format. The first matching format is used, and if none matches, MPD chooses the best fallback of this list.
       
       Example: "96000:16:* 192000:24:* dsd64:*=dop *:dsd:*".

The according hardware mixer plugin understands the following settings:

.. list-table::
   :widths: 20 80
   :header-rows: 1

   * - Setting
     - Description
   * - **mixer_device DEVICE**
     - Sets the ALSA mixer device name, defaulting to default which lets ALSA pick a value.
   * - **mixer_control NAME**
     - Choose a mixer control, defaulting to PCM. Type amixer scontrols to get a list of available mixer controls.
   * - **mixer_index NUMBER**
     - Choose a mixer control index. This is necessary if there is more than one control with the same name. Defaults to 0 (the first one).

The following attributes can be configured at runtime using the outputset command:

.. list-table::
   :widths: 20 80
   :header-rows: 1

   * - Setting
     - Description
   * - **dop 1|0**
     - Allows changing the dop configuration setting at runtime. This takes effect the next time the output is opened.
   * - **allowed_formats F1 F2 ...**
     - Allows changing the allowed_formats configuration setting at runtime. This takes effect the next time the output is opened.


ao
--
The ao plugin uses the portable `libao <https://www.xiph.org/ao/>`_ library. Use only if there is no native plugin for your operating system.

.. list-table::
   :widths: 20 80
   :header-rows: 1

   * - Setting
     - Description
   * - **driver D**
     - The libao driver to use for audio output. Possible values depend on what libao drivers are available. See http://www.xiph.org/ao/doc/drivers.html for information on some commonly used drivers. Typical values for Linux include "oss" and "alsa09". The default is "default", which causes libao to select an appropriate plugin.
   * - **options O**
     - Options to pass to the selected libao driver.
   * - **write_size O**
     - This specifies how many bytes to write to the audio device at once. This parameter is to work around a bug in older versions of libao on sound cards with very small buffers. The default is 1024.

sndio
-----

The sndio plugin uses the `sndio <http://www.sndio.org/>`_ library. It should normally be used on OpenBSD.

.. list-table::
   :widths: 20 80
   :header-rows: 1

   * - Setting
     - Description
   * - **device NAME**
     - The audio output device libsndio will attempt to use. The default is "default" which causes libsndio to select the first output device.
   * - **buffer_time MS**
     - Set the application buffer time in milliseconds.

fifo
----

The fifo plugin writes raw PCM data to a FIFO (First In, First Out) file. The data can be read by another program.

.. list-table::
   :widths: 20 80
   :header-rows: 1

   * - Setting
     - Description
   * - **path P**
     - This specifies the path of the FIFO to write to. Must be an absolute path. If the path does not exist, it will be created when MPD is started, and removed when MPD is stopped. The FIFO will be created with the same user and group as MPD is running as. Default permissions can be modified by using the builtin shell command umask. If a FIFO already exists at the specified path it will be reused, and will not be removed when MPD is stopped. You can use the "mkfifo" command to create this, and then you may modify the permissions to your liking.

haiku
-----

Use the SoundPlayer API on the Haiku operating system.

This plugin is unmaintained and contains known bugs.  It will be
removed soon, unless there is a new maintainer.


jack
----

The jack plugin connects to a `JACK server <http://jackaudio.org/>`_.

On Windows, this plugin loads :file:`libjack64.dll` at runtime.  This
means you need to `download and install the JACK windows build
<https://jackaudio.org/downloads/>`_.

.. list-table::
   :widths: 20 80
   :header-rows: 1

   * - Setting
     - Description
   * - **client_name NAME**
     - The name of the JACK client. Defaults to "Music Player Daemon".
   * - **server_name NAME**
     - Optional name of the JACK server.
   * - **autostart yes|no**
     - If set to yes, then libjack will automatically launch the JACK daemon. Disabled by default.
   * - **source_ports A,B**
     - The names of the JACK source ports to be created. By default, the ports "left" and "right" are created. To use more ports, you have to tweak this option.
   * - **destination_ports A,B**
     - The names of the JACK destination ports to connect to.
   * - **auto_destination_ports yes|no**
     - If set to *yes*, then MPD will automatically create connections between the send ports of
       MPD and receive ports of the first sound card; if set to *no*, then MPD will only create
       connections to the contents of *destination_ports* if it is set. Enabled by default.
   * - **ringbuffer_size NBYTES**
     - Sets the size of the ring buffer for each channel. Do not configure this value unless you know what you're doing.

httpd
-----

The httpd plugin creates a HTTP server, similar to `ShoutCast <http://www.shoutcast.com/>`_ / `IceCast <http://icecast.org/>`_. HTTP streaming clients like mplayer, VLC, and mpv can connect to it.

It is highly recommended to configure a fixed format, because a stream cannot switch its audio format on-the-fly when the song changes.

.. list-table::
   :widths: 20 80
   :header-rows: 1

   * - Setting
     - Description
   * - **port P**
     - Binds the HTTP server to the specified port.
   * - **bind_to_address ADDR**
     - Binds the HTTP server to the specified address (IPv4, IPv6 or local socket). Multiple addresses in parallel are not supported.
   * - **dscp_class CLASS**
     - Sets a DSCP (`Differentiated Services Code Point
       <https://en.wikipedia.org/wiki/Differentiated_services>`__)
       class for outgoing traffic.  This can either be a name
       (``CS*``, ``LE``, ``AF*``, ``EF``) or numeric (decimal or
       hexadecimal).  A reasonable choice for this setting is ``CS3``
       ("broadcast video").
   * - **encoder NAME**
     - Chooses an encoder plugin. A list of encoder plugins can be found in the encoder plugin reference :ref:`encoder_plugins`.
   * - **max_clients MC**
     - Sets a limit, number of concurrent clients. When set to 0 no limit will apply.

null
----

The null plugin does nothing. It discards everything sent to it.

.. list-table::
   :widths: 20 80
   :header-rows: 1

   * - Setting
     - Description
   * - **sync yes|no**
     - If set to no, then the timer is disabled - the device will accept PCM chunks at arbitrary rate (useful for benchmarking). The default behaviour is to play in real time.

.. _oss_plugin:

oss
---

The "Open Sound System" plugin is supported on most Unix platforms.

On Linux, OSS has been superseded by ALSA. Use the ALSA output plugin :ref:`alsa_plugin` instead of this one on Linux.

.. list-table::
   :widths: 20 80
   :header-rows: 1

   * - Setting
     - Description
   * - **device PATH**
     - Sets the path of the PCM device. If not specified, then MPD will attempt to open /dev/sound/dsp and /dev/dsp.
   * - **dop yes|no**
     - If set to yes, then DSD over PCM according to the `DoP standard <http://dsd-guide.com/dop-open-standard>`_ is enabled. This wraps DSD samples in fake 24 bit PCM, and is understood by some DSD capable products, but may be harmful to other hardware. Therefore, the default is no and you can enable the option at your own risk.

The according hardware mixer plugin understands the following settings:

.. list-table::
   :widths: 20 80
   :header-rows: 1

   * - Setting
     - Description
   * - **mixer_device DEVICE**
     - Sets the OSS mixer device path, defaulting to /dev/mixer.
   * - **mixer_control NAME**
     - Choose a mixer control, defaulting to PCM.

openal
------
The "OpenAL" plugin uses `libopenal <http://kcat.strangesoft.net/openal.html>`_. It is supported on many platforms. Use only if there is no native plugin for your operating system.

.. list-table::
   :widths: 20 80
   :header-rows: 1

   * - Setting
     - Description
   * - **device NAME**
     - Sets the device which should be used. This can be any valid OpenAL device name. If not specified, then libopenal will choose a default device.

osx
---
The "Mac OS X" plugin uses Apple's CoreAudio API.

.. list-table::
   :widths: 20 80
   :header-rows: 1

   * - Setting
     - Description
   * - **device NAME**
     - Sets the device which should be used. Uses device names as listed in the "Audio Devices" window of "Audio MIDI Setup".
   * - **hog_device yes|no**
     - Hog the device. This means that it takes exclusive control of the audio output device it is playing through, and no other program can access it.
   * - **dop yes|no**
     - If set to yes, then DSD over PCM according to the `DoP standard <http://dsd-guide.com/dop-open-standard>`_ is enabled. This wraps DSD samples in fake 24 bit PCM, and is understood by some DSD capable products, but may be harmful to other hardware. Therefore, the default is no and you can enable the option at your own risk. Under macOS you must make sure to select a physical mode on the output device which supports at least 24 bits per channel as the Mac OS X plugin only changes the sample rate.
   * - **channel_map SOURCE,SOURCE,...**
     - Specifies a channel map. If your audio device has more than two outputs this allows you to route audio to auxillary outputs. For predictable results you should also specify a "format" with a fixed number of channels, e.g. "*:*:2". The number of items in the channel map must match the number of output channels of your output device. Each list entry specifies the source for that output channel; use "-1" to silence an output. For example, if you have a four-channel output device and you wish to send stereo sound (format "*:*:2") to outputs 3 and 4 while leaving outputs 1 and 2 silent then set the channel map to "-1,-1,0,1". In this example '0' and '1' denote the left and right channel respectively.

       The channel map may not refer to outputs that do not exist according to the format. If the format is "*:*:1" (mono) and you have a four-channel sound card then "-1,-1,0,0" (dual mono output on the second pair of sound card outputs) is a valid channel map but "-1,-1,0,1" is not because the second channel ('1') does not exist when the output is mono.

pipe
----

The pipe plugin starts a program and writes raw PCM data into its standard input.

.. list-table::
   :widths: 20 80
   :header-rows: 1

   * - Setting
     - Description
   * - **command CMD**
     - This command is invoked with the shell.

pipewire
--------

Connect to a `PipeWire <https://pipewire.org/>`_ server.  Requires
``libpipewire``.

.. list-table::
   :widths: 20 80
   :header-rows: 1

   * - Setting
     - Description
   * - **target NAME**
     - Link to the given target.  If not specified, let the PipeWire
       manager select a target.  To get a list of available targets,
       type ``pw-cli dump short Node``
   * - **remote NAME**
     - The name of the remote to connect to.  The default is
       ``pipewire-0``.
   * - **dsd yes|no**
     - Enable DSD playback.  This requires PipeWire 0.38.

.. _pulse_plugin:

pulse
-----
The pulse plugin connects to a `PulseAudio <http://www.freedesktop.org/wiki/Software/PulseAudio/>`_ server. Requires libpulse.

.. list-table::
   :widths: 20 80
   :header-rows: 1

   * - Setting
     - Description
   * - **server HOSTNAME**
     - Sets the host name of the PulseAudio server. By default, :program:`MPD` connects to the local PulseAudio server.
   * - **sink NAME**
     - Specifies the name of the PulseAudio sink :program:`MPD` should play on.
   * - **media_role ROLE**
     - Specifies a custom media role that :program:`MPD` reports to PulseAudio. Default is "music". (optional).
   * - **scale_volume FACTOR**
     - Specifies a linear scaling coefficient (ranging from 0.5 to 5.0) to apply when adjusting volume through :program:`MPD`.  For example, chosing a factor equal to ``"0.7"`` means that setting the volume to 100 in :program:`MPD` will set the PulseAudio volume to 70%, and a factor equal to ``"3.5"`` means that volume 100 in :program:`MPD` corresponds to a 350% PulseAudio volume.

recorder
--------
The recorder plugin writes the audio played by :program:`MPD` to a file. This may be useful for recording radio streams.

.. list-table::
   :widths: 20 80
   :header-rows: 1

   * - Setting
     - Description
   * - **path P**
     - Write to this file.
   * - **format_path P**
     - An alternative to path which provides a format string referring to tag values. The special tag iso8601 emits the current date and time in `ISO8601 <https://en.wikipedia.org/wiki/ISO_8601>`_ format (UTC). Every time a new song starts or a new tag gets received from a radio station, a new file is opened. If the format does not render a file name, nothing is recorded. A tag name enclosed in percent signs ('%') is replaced with the tag value. Example: :file:`-/.mpd/recorder/%artist% - %title%.ogg`. Square brackets can be used to group a substring. If none of the tags referred in the group can be found, the whole group is omitted. Example: [-/.mpd/recorder/[%artist% - ]%title%.ogg] (this omits the dash when no artist tag exists; if title also doesn't exist, no file is written). The operators "|" (logical "or") and "&" (logical "and") can be used to select portions of the format string depending on the existing tag values. Example: -/.mpd/recorder/[%title%|%name%].ogg (use the "name" tag if no title exists)
   * - **encoder NAME**
     - Chooses an encoder plugin. A list of encoder plugins can be found in the encoder plugin reference :ref:`encoder_plugins`.


shout
-----
The shout plugin connects to a ShoutCast or IceCast server using libshout. It forwards tags to this server.

You must set a format.

.. list-table::
   :widths: 20 80
   :header-rows: 1

   * - Setting
     - Description
   * - **host HOSTNAME**
     - Sets the host name of the `ShoutCast <http://www.shoutcast.com/>`_ / `IceCast <http://icecast.org/>`_ server.
   * - **port PORTNUMBER**
     - Connect to this port number on the specified host.
   * - **protocol icecast2|icecast1|shoutcast**
     - Specifies the protocol that wil be used to connect to the server. The default is "icecast2".
   * - **tls disabled|auto|auto_no_plain|rfc2818|rfc2817**
     - Specifies what kind of TLS to use. The default is "disabled" (no TLS).
   * - **mount URI**
     - Mounts the :program:`MPD` stream in the specified URI.
   * - **user USERNAME**
     - Sets the user name for submitting the stream to the server. Default is "source".
   * - **password PWD**
     - Sets the password for submitting the stream to the server.
   * - **name NAME**
     - Sets the name of the stream.
   * - **genre GENRE**
     - Sets the genre of the stream (optional).
   * - **description DESCRIPTION**
     - Sets a short description of the stream (optional).
   * - **url URL**
     - Sets a URL associated with the stream (optional).
   * - **public yes|no**
     - Specifies whether the stream should be "public". Default is no.
   * - **encoder PLUGIN**
     - Chooses an encoder plugin. Default is vorbis :ref:`vorbis_plugin`. A list of encoder plugins can be found in the encoder plugin reference :ref:`encoder_plugins`.


.. _sles_output:

sles
----

Plugin using the `OpenSL ES <https://www.khronos.org/opensles/>`__
audio API.  Its primary use is local playback on Android, where
:ref:`ALSA <alsa_plugin>` is not available.  It supports 16 bit and
floating point samples.


snapcast
--------

Snapcast is a multiroom client-server audio player.  This plugin
allows MPD to act as a `Snapcast
<https://github.com/badaix/snapcast>`__ server.  Snapcast clients
connect to it and receive audio data from MPD.

You must set a format.

.. list-table::
   :widths: 20 80
   :header-rows: 1

   * - Setting
     - Description
   * - **port P**
     - Binds the Snapcast server to the specified port.  The default
       port is :samp:`1704`.
   * - **bind_to_address ADDR**
     - Binds the Snapcast server to the specified address.  Multiple
       addresses in parallel are not supported.  The default is to
       bind on all addresses on port :samp:`1704`.
   * - **zeroconf yes|no**
     - Publish the Snapcast server as service type ``_snapcast._tcp``
       via Zeroconf (Avahi or Bonjour).  Default is :samp:`yes`.


solaris
-------
The "Solaris" plugin runs only on SUN Solaris, and plays via /dev/audio.

.. list-table::
   :widths: 20 80
   :header-rows: 1

   * - Setting
     - Description
   * - **device PATH**
     - Sets the path of the audio device, defaults to /dev/audio.


wasapi
------

The `Windows Audio Session API <https://docs.microsoft.com/en-us/windows/win32/coreaudio/wasapi>`_ plugin uses WASAPI, which is supported started from Windows Vista. It is recommended if you are using Windows.

.. list-table::
   :widths: 20 80
   :header-rows: 1

   * - Setting
     - Description
   * - **device NAME**
     - Sets the device which should be used. This can be any valid audio device name, or index number. The default value is "", which makes WASAPI choose the default output device.
   * - **enumerate yes|no**
     - Enumerate all devices in log while playing started. Useful for device configuration. The default value is "no".
   * - **exclusive yes|no**
     - Exclusive mode blocks all other audio source, and get best audio quality without resampling. Stopping playing release the exclusive control of the output device. The default value is "no".
   * - **dop yes|no**
     - Enable DSD over PCM. Require exclusive mode. The default value is "no".


.. _filter_plugins:

Filter plugins
==============

ffmpeg
------

Configures a FFmpeg filter graph.

This plugin requires building with ``libavfilter`` (FFmpeg).

.. list-table::
   :widths: 20 80
   :header-rows: 1

   * - Setting
     - Description
   * - **graph "..."**
     - Specifies the ``libavfilter`` graph; read the `FFmpeg
       documentation
       <https://ffmpeg.org/ffmpeg-filters.html#Filtergraph-syntax-1>`_
       for details


hdcd
----

Decode `HDCD
<https://en.wikipedia.org/wiki/High_Definition_Compatible_Digital>`_.

This plugin requires building with ``libavfilter`` (FFmpeg).

normalize
---------

Normalize the volume during playback (at the expense of quality).


null
----

A no-op filter.  Audio data is returned as-is.


route
-----

Reroute channels.

.. list-table::
   :widths: 20 80
   :header-rows: 1

   * - Setting
     - Description
   * - **routes "0>0, 1>1, ..."**
     - Specifies the channel mapping.


.. _playlist_plugins:

Playlist plugins
================

asx
---

Reads :file:`.asx` playlist files.

.. _cue_playlist:

cue
---
Reads :file:`.cue` files.

embcue
------
Reads CUE sheets from the ``CUESHEET`` tag of song files.

m3u
---
Reads :file:`.m3u` playlist files.

extm3u
------
Reads extended :file:`.m3u` playlist files.

flac
----
Reads the cuesheet metablock from a FLAC file.

pls
---
Reads :file:`.pls` playlist files.

rss
---
Reads music links from :file:`.rss` files.

soundcloud
----------

Download playlist from SoundCloud. It accepts URIs starting with soundcloud://.

.. list-table::
   :widths: 20 80
   :header-rows: 1

   * - Setting
     - Description
   * - **apikey KEY**
     - An API key to access the SoundCloud servers.

xspf
----
Reads XSPF playlist files. 


.. _archive_plugins:

Archive plugins
===============

bz2
---
Allows to load single bzip2 compressed files using `libbz2 <https://www.sourceware.org/bzip2/>`_. Does not support seeking.

zzip
----
Allows to load music files from ZIP archives using `zziplib <http://zziplib.sourceforge.net/>`_.

iso
---
Allows to load music files from ISO 9660 images using `libcdio <https://www.gnu.org/software/libcdio/>`_.
