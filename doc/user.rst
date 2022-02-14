User's Manual
#############

Introduction
************

Music Player Daemon (:program:`MPD`) is a flexible, powerful, server-side application for playing music. Through plugins and libraries it can play a variety of sound files while being controlled by its network protocol.

This document is work in progress. Most of it may be incomplete yet. Please help!

Installation
************

Installing on Debian/Ubuntu
---------------------------

Install the package :file:`mpd` via :program:`apt`:

.. code-block:: none

    apt install mpd

When installed this way, :program:`MPD` by default looks for music in :file:`/var/lib/mpd/music/`; this may not be correct. Look at your :file:`/etc/mpd.conf` file... 

.. note::

   Debian and Ubuntu are infamous for shipping heavily outdated
   software.  The :program:`MPD` version in their respective stable
   releases are usually too old to be supported by this project.
   Ironically, the :program:`MPD` version in Debian "*unstable*" is
   more stable than the version in Debian "*stable*".


Installing on Android
---------------------

An experimental Android build is available on Google Play. After installing and launching it, :program:`MPD` will scan the music in your Music directory and you can control it as usual with a :program:`MPD` client.

If you need to tweak the configuration, you can create a file called :file:`mpd.conf` on the data partition (the directory which is returned by Android's :dfn:`getExternalStorageDirectory()` API function). 

ALSA is not available on Android; only the :ref:`OpenSL ES
<sles_output>` output plugin can be used for local playback.

Compiling from source
---------------------

`Download the source tarball <https://www.musicpd.org/download.html>`_
and unpack it (or `clone the git repository
<https://github.com/MusicPlayerDaemon/MPD>`_):

.. code-block:: none

    tar xf mpd-version.tar.xz
    cd mpd-version

In any case, you need:

* a C++17 compiler (e.g. GCC 8 or clang 7)
* `Meson 0.56.0 <http://mesonbuild.com/>`__ and `Ninja
  <https://ninja-build.org/>`__
* Boost 1.58
* pkg-config 

Each plugin usually needs a codec library, which you also need to
install. Check the :doc:`plugins` for details about required libraries

For example, the following installs a fairly complete list of build dependencies on Debian Bullseye:

.. code-block:: none

    apt install meson g++ \
      libfmt-dev \
      libpcre2-dev \
      libmad0-dev libmpg123-dev libid3tag0-dev \
      libflac-dev libvorbis-dev libopus-dev libogg-dev \
      libadplug-dev libaudiofile-dev libsndfile1-dev libfaad-dev \
      libfluidsynth-dev libgme-dev libmikmod-dev libmodplug-dev \
      libmpcdec-dev libwavpack-dev libwildmidi-dev \
      libsidplay2-dev libsidutils-dev libresid-builder-dev \
      libavcodec-dev libavformat-dev \
      libmp3lame-dev libtwolame-dev libshine-dev \
      libsamplerate0-dev libsoxr-dev \
      libbz2-dev libcdio-paranoia-dev libiso9660-dev libmms-dev \
      libzzip-dev \
      libcurl4-gnutls-dev libyajl-dev libexpat-dev \
      libasound2-dev libao-dev libjack-jackd2-dev libopenal-dev \
      libpulse-dev libshout3-dev \
      libsndio-dev \
      libmpdclient-dev \
      libnfs-dev \
      libupnp-dev \
      libavahi-client-dev \
      libsqlite3-dev \
      libsystemd-dev \
      libgtest-dev \
      libboost-dev \
      libicu-dev \
      libchromaprint-dev \
      libgcrypt20-dev
      

Now configure the source tree:

.. code-block:: none

 meson . output/release --buildtype=debugoptimized -Db_ndebug=true

The following command shows a list of compile-time options:

.. code-block:: none

 meson configure output/release

NB: Check the sysconfdir setting to determine where mpd will look for mpd.conf; if you expect mpd to look for /etc/mpd.conf the sysconfdir must be '/etc' (i.e., not 'etc' which will result in mpd looking for /usr/local/etc/mpd.conf):
 
.. code-block:: none
 
 meson configure output/release |grep sysconfdir

If this is not /etc (or another path you wish to specify):

.. code-block:: none

 $ meson configure output/release -Dsysconfdir='/etc' ; meson configure output/release |grep syscon
  sysconfdir              /etc                                               Sysconf data directory

When everything is ready and configured, compile:

.. code-block:: none

 ninja -C output/release

And install:

.. code-block:: none

 ninja -C output/release install

Compiling for Windows
---------------------

Even though it does not "feel" like a Windows application, :program:`MPD` works well under Windows. Its build process follows the "Linux style" and may seem awkward for Windows people (who are not used to compiling their software, anyway).

Basically, there are two ways to compile :program:`MPD` for Windows:

* Build as described above: with :program:`meson` and
  :program:`ninja`.  To cross-compile from Linux, you need `a Meson
  cross file <https://mesonbuild.com/Cross-compilation.html>`__.

  The remaining difficulty is installing all the external libraries.
  And :program:`MPD` usually needs many, making this method cumbersome
  for the casual user.

* Build on Linux for Windows using :program:`MPD`'s library build script. 

This section is about the latter.

You need:

* `mingw-w64 <http://mingw-w64.org/doku.php>`__
* `Meson 0.56.0 <http://mesonbuild.com/>`__ and `Ninja
  <https://ninja-build.org/>`__
* cmake
* pkg-config
* quilt

Just like with the native build, unpack the :program:`MPD` source
tarball and change into the directory.  Then, instead of
:program:`meson`, type:

.. code-block:: none

 mkdir -p output/win64
 cd output/win64
 ../../win32/build.py --64 \
   --buildtype=debugoptimized -Db_ndebug=true \
   -Dwrap_mode=forcefallback

This downloads various library sources, and then configures and builds
:program:`MPD` (for x64; to build a 32 bit binary, pass
:code:`--32`). The resulting EXE files is linked statically, i.e. it
contains all the libraries already and you do not need carry DLLs
around. It is large, but easy to use. If you wish to have a small
mpd.exe with DLLs, you need to compile manually, without the
:file:`build.py` script.

The option ``-Dwrap_mode=forcefallback`` tells Meson to download and
cross-compile several libraries used by MPD instead of looking for
them on your computer.


Compiling for Android
---------------------

:program:`MPD` can be compiled as an Android app. It can be installed easily with Google Play, but if you want to build it from source, follow this section.

You need:

* Android SDK
* `Android NDK r23 <https://developer.android.com/ndk/downloads>`_
* `Meson 0.56.0 <http://mesonbuild.com/>`__ and `Ninja
  <https://ninja-build.org/>`__
* cmake
* pkg-config
* quilt

Just like with the native build, unpack the :program:`MPD` source
tarball and change into the directory.  Then, instead of
:program:`meson`, type:

.. code-block:: none

 mkdir -p output/android
 cd output/android
 ../../android/build.py SDK_PATH NDK_PATH ABI \
   --buildtype=debugoptimized -Db_ndebug=true \
   -Dwrap_mode=forcefallback \
   -Dandroid_debug_keystore=$HOME/.android/debug.keystore
 ninja android/apk/mpd-debug.apk

:envvar:`SDK_PATH` is the absolute path where you installed the
Android SDK; :envvar:`NDK_PATH` is the Android NDK installation path;
ABI is the Android ABI to be built, e.g. ":code:`arm64-v8a`".

This downloads various library sources, and then configures and builds :program:`MPD`. 

Configuration
*************

The Configuration File
----------------------

:program:`MPD` reads its configuration from a text file. Usually, that is :file:`/etc/mpd.conf`, unless a different path is specified on the command line. If you run :program:`MPD` as a user daemon (and not as a system daemon), the configuration is read from :file:`$XDG_CONFIG_HOME/mpd/mpd.conf` (usually :file:`~/.config/mpd/mpd.conf`). On Android, :file:`mpd.conf` will be loaded from the top-level directory of the data partition.

Each line in the configuration file contains a setting name and its value, e.g.:

:code:`connection_timeout "5"`

For settings which specify a filesystem path, the tilde is expanded:

:code:`music_directory "~/Music"`

Some of the settings are grouped in blocks with curly braces, e.g. per-plugin settings:

.. code-block:: none

    audio_output {
        type "alsa"
        name "My ALSA output"
        device "iec958:CARD=Intel,DEV=0"
        mixer_control "PCM"
    }

The :code:`include` directive can be used to include settings from
another file; the given file name is relative to the current file:

.. code-block:: none

  include "other.conf"

You can use :code:`include_optional` instead if you want the included file
to be optional; the directive will be ignored if the file does not exist:

.. code-block:: none

  include_optional "may_not_exist.conf"

Configuring the music directory
-------------------------------

When you play local files, you should organize them within a directory called the "music directory". This is configured in :program:`MPD` with the music_directory setting.

By default, :program:`MPD` follows symbolic links in the music directory. This behavior can be switched off: :code:`follow_outside_symlinks` controls whether :program:`MPD` follows links pointing to files outside of the music directory, and :code:`follow_inside_symlinks` lets you disable symlinks to files inside the music directory.

Instead of using local files, you can use storage plugins to access
files on a remote file server. For example, to use music from the
SMB/CIFS server ":file:`myfileserver`" on the share called "Music",
configure the music directory ":file:`smb://myfileserver/Music`". For
a recipe, read the Satellite :program:`MPD` section :ref:`satellite`.

You can also use multiple storage plugins to assemble a virtual music directory consisting of multiple storages. 

Configuring database plugins
----------------------------

If a music directory is configured, one database plugin is used. To
configure this plugin, add a :code:`database` block to
:file:`mpd.conf`:

.. code-block:: none

    database {
        plugin "simple"
        path "/var/lib/mpd/db"
    }
    
More information can be found in the :ref:`database_plugins`
reference.

Configuring neighbor plugins
----------------------------

All neighbor plugins are disabled by default to avoid unwanted
overhead. To enable (and configure) a plugin, add a :code:`neighbor`
block to :file:`mpd.conf`:

.. code-block:: none

    neighbors {
        plugin "smbclient"
    }
      
More information can be found in the :ref:`neighbor_plugin` reference.

Configuring input plugins
-------------------------

To configure an input plugin, add an :code:`input` block to
:file:`mpd.conf`:

.. code-block:: none

    input {
        plugin "curl"
        proxy "proxy.local"
    }
      

The following table lists the input options valid for all plugins:

.. list-table::
   :widths: 20 80
   :header-rows: 1

   * - Name
     - Description
   * - **plugin**
     - The name of the plugin
   * - **enabled yes|no**
     - Allows you to disable a input plugin without recompiling. By default, all plugins are enabled.

More information can be found in the :ref:`input_plugins` reference.

Configuring archive plugins
---------------------------

To configure an archive plugin, add an :code:`archive_plugin` block to
:file:`mpd.conf`:

.. code-block:: none

    archive_plugin {
        name "zzip"
        enabled "no"
    }

The following table lists the input options valid for all plugins:

.. list-table::
   :widths: 20 80
   :header-rows: 1

   * - Name
     - Description
   * - **name**
     - The name of the plugin
   * - **enabled yes|no**
     - Allows you to disable a plugin without recompiling.  By
       default, all plugins are enabled.

More information can be found in the :ref:`archive_plugins` reference.

.. _input_cache:

Configuring the Input Cache
^^^^^^^^^^^^^^^^^^^^^^^^^^^

The input cache prefetches queued song files before they are going to
be played.  This has several advantages:

- risk of buffer underruns during playback is reduced because this
  decouples playback from disk (or network) I/O
- bulk transfers may be faster and more energy efficient than loading
  small chunks on-the-fly
- by prefetching several songs at a time, the hard disk can spin down
  for longer periods of time

This comes at a cost:

- memory usage
- bulk transfers may reduce the performance of other applications
  which also want to access the disk (if the kernel's I/O scheduler
  isn't doing its job properly)

To enable the input cache, add an ``input_cache`` block to the
configuration file:

.. code-block:: none

    input_cache {
        size "1 GB"
    }

This allocates a cache of 1 GB.  If the cache grows larger than that,
older files will be evicted.

You can flush the cache at any time by sending ``SIGHUP`` to the
:program:`MPD` process, see :ref:`signals`.


Configuring decoder plugins
---------------------------

Most decoder plugins do not need any special configuration. To
configure a decoder, add a :code:`decoder` block to :file:`mpd.conf`:

.. code-block:: none

    decoder {
        plugin "wildmidi"
        config_file "/etc/timidity/timidity.cfg"
    }
      
The following table lists the decoder options valid for all plugins:

.. list-table::
   :widths: 20 80
   :header-rows: 1

   * - Name
     - Description
   * - **plugin**
     - The name of the plugin
   * - **enabled yes|no**
     - Allows you to disable a decoder plugin without recompiling. By default, all plugins are enabled.

More information can be found in the :ref:`decoder_plugins` reference.

Configuring encoder plugins
---------------------------

Encoders are used by some of the output plugins (such as shout). The
encoder settings are included in the ``audio_output`` section, see :ref:`config_audio_output`.

More information can be found in the :ref:`encoder_plugins` reference.


.. _config_audio_output:

Configuring audio outputs
-------------------------

Audio outputs are devices which actually play the audio chunks produced by :program:`MPD`. You can configure any number of audio output devices, but there must be at least one. If none is configured, :program:`MPD` attempts to auto-detect. Usually, this works quite well with ALSA, OSS and on Mac OS X.

To configure an audio output manually, add one or more
:code:`audio_output` blocks to :file:`mpd.conf`:

.. code-block:: none

    audio_output {
        type "alsa"
        name "my ALSA device"
        device "hw:0"
    }

The following table lists the audio_output options valid for all plugins:


.. list-table::
   :widths: 20 80
   :header-rows: 1

   * - Name
     - Description
   * - **type**
     - The name of the plugin
   * - **name**
     - The name of the audio output. It is visible to the client. Some plugins also use it internally, e.g. as a name registered in the PULSE server.
   * - **format samplerate:bits:channels**
     -  Always open the audio output with the specified audio format, regardless of the format of the input file. This is optional for most plugins.
        See :ref:`audio_output_format` for a detailed description of the value.
   * - **enabled yes|no**
     - Specifies whether this audio output is enabled when :program:`MPD` is started. By default, all audio outputs are enabled. This is just the default setting when there is no state file; with a state file, the previous state is restored.
   * - **tags yes|no**
     - If set to no, then :program:`MPD` will not send tags to this output. This is only useful for output plugins that can receive tags, for example the httpd output plugin.
   * - **always_on yes|no**
     - If set to yes, then :program:`MPD` attempts to keep this audio output always open. This may be useful for streaming servers, when you don't want to disconnect all listeners even when playback is accidentally stopped.
   * - **mixer_type hardware|software|null|none**
     - Specifies which mixer should be used for this audio output: the
       hardware mixer (available for ALSA :ref:`alsa_plugin`, OSS
       :ref:`oss_plugin` and PulseAudio :ref:`pulse_plugin`), the
       software mixer, the ":samp:`null`" mixer (allows setting the
       volume, but with no effect; this can be used as a trick to
       implement an external mixer, see :ref:`external_mixer`) or no mixer
       (:samp:`none`). By default, the hardware mixer is used for
       devices which support it, and none for the others.
   * - **replay_gain_handler software|mixer|none**
     - Specifies how :ref:`replay_gain` is applied.  The default is
       ``software``, which uses an internal software volume control.
       ``mixer`` uses the configured (hardware) mixer control.
       ``none`` disables replay gain on this audio output.
   * - **filters "name,...**"
     - The specified configured filters are instantiated in the given
       order.  Each filter name refers to a ``filter`` block, see
       :ref:`config_filter`.

More information can be found in the :ref:`output_plugins` reference.


.. _config_filter:

Configuring filters
-------------------

Filters are plugins which modify an audio stream.

To configure a filter, add a :code:`filter` block to :file:`mpd.conf`:

.. code-block:: none

    filter {
        plugin "volume"
        name "software volume"
    }

Configured filters may then be added to the ``filters`` setting of an
``audio_output`` section, see :ref:`config_audio_output`.

The following table lists the filter options valid for all plugins:

.. list-table::
   :widths: 20 80
   :header-rows: 1

   * - Name
     - Description
   * - **plugin**
     - The name of the plugin
   * - **name**
     - The name of the filter

More information can be found in the :ref:`filter_plugins` reference.


Configuring playlist plugins
----------------------------

Playlist plugins are used to load remote playlists (protocol commands
load, listplaylist and listplaylistinfo). This is not related to
:program:`MPD`'s :ref:`playlist directory <stored_playlists>`.

To configure a playlist plugin, add a :code:`playlist_plugin` block to
:file:`mpd.conf`:

.. code-block:: none

    playlist_plugin {
        name "m3u"
        enabled "true"
    }

The following table lists the playlist_plugin options valid for all plugins:

.. list-table::
   :widths: 20 80
   :header-rows: 1

   * - Name
     - Description
   * - **plugin**
     - The name of the plugin
   * - **enabled yes|no**
     - Allows you to disable a playlist plugin without recompiling. By default, all plugins are enabled.
   * - **as_directory yes|no**
     - With this option, a playlist file of this type is parsed during
       database update and converted to a virtual directory, allowing
       MPD clients to access individual entries.  By default, this is
       only enabled for the :ref:`cue plugin <cue_playlist>`.

More information can be found in the :ref:`playlist_plugins`
reference.

Audio Format Settings
---------------------

.. _audio_output_format:

Global Audio Format
^^^^^^^^^^^^^^^^^^^

The setting ``audio_output_format`` forces :program:`MPD` to use one
audio format for all outputs.  Doing that is usually not a good idea.

The value is specified as ``samplerate:bits:channels``.

Any of the three attributes may be an asterisk to specify that this
attribute should not be enforced, example: ``48000:16:*``.
``*:*:*`` is equal to not having a format specification.

The following values are valid for bits: ``8`` (signed 8 bit integer
samples), ``16``, ``24`` (signed 24 bit integer samples padded to 32
bit), ``32`` (signed 32 bit integer samples), ``f`` (32 bit floating
point, -1.0 to 1.0), ``dsd`` means DSD (Direct Stream Digital). For
DSD, there are special cases such as ``dsd64``, which allows you to
omit the sample rate (e.g. ``dsd512:2`` for stereo DSD512,
i.e. 22.5792 MHz).

The sample rate is special for DSD: :program:`MPD` counts the number
of bytes, not bits. Thus, a DSD "bit" rate of 22.5792 MHz (DSD512) is
2822400 from :program:`MPD`'s point of view (44100*512/8).

Resampler
^^^^^^^^^

Sometimes, music needs to be resampled before it can be played; for example, CDs use a sample rate of 44,100 Hz while many cheap audio chips can only handle 48,000 Hz. Resampling reduces the quality and consumes a lot of CPU. There are different options, some of them optimized for high quality and others for low CPU usage, but you can't have both at the same time. Often, the resampler is the component that is responsible for most of :program:`MPD`'s CPU usage. Since :program:`MPD` comes with high quality defaults, it may appear that :program:`MPD` consumes more CPU than other software.

Check the :ref:`resampler_plugins` reference for a list of resamplers
and how to configure them.

Volume Normalization Settings
-----------------------------

.. _replay_gain:

Replay Gain
^^^^^^^^^^^

The setting ``replaygain`` specifies whether MPD shall adjust the
volume of songs played using `ReplayGain
<https://wiki.hydrogenaud.io/index.php?title=Replaygain>`__ tags.
Setting this to ``album`` will adjust volume using the album's
ReplayGain tags, while setting it to ``track`` will adjust it using
the "track" ReplayGain tags.  ``auto`` uses the track ReplayGain tags
if random play is activated otherwise the album ReplayGain
tags.

If ReplayGain is enabled, then the setting ``replaygain_preamp`` is
set to a value (in dB) between ``-15`` and ``15``.  This is the gain
applied to songs with ReplayGain tags.

ReplayGain is usually implemented with a software volume filter (which
prevents `Bit-perfect playback`_).  To use a hardware mixer, set
``replay_gain_handler`` to ``mixer`` in the ``audio_output`` section
(see :ref:`config_audio_output` for details).

Simple Volume Normalization
^^^^^^^^^^^^^^^^^^^^^^^^^^^

MPD implements a very simple volume normalization method which can be
enabled by setting ``volume_normalization`` to ``yes``.  It supports
16 bit PCM only.


.. _crossfading:

Cross-Fading
------------

If ``crossfade`` is set to a positive number, then adjacent songs are
cross-faded by this number of seconds.  This is a run-time setting
:ref:`which can be controlled by clients <command_crossfade>`,
e.g. with :program:`mpc`::

  mpc crossfade 10
  mpc crossfade 0

Zero means cross-fading is disabled.

Cross-fading is only possible if both songs have the same audio
format.  At the cost of quality loss and higher CPU usage, you can
make sure this is always given by configuring
:ref:`audio_output_format`.

.. _mixramp:

MixRamp
^^^^^^^

MixRamp tags describe the loudness levels at start and end of a song
and can be used by MPD to find the best time to begin cross-fading.
MPD enables MixRamp if:

- Cross-fade is enabled
- :ref:`mixrampdelay <command_mixrampdelay>` is set to a positive
  value, e.g.::
    mpc mixrampdelay 1
- :ref:`mixrampdb <command_mixrampdb>` is set to a reasonable value,
  e.g.::
    mpc mixrampdb -17
- both songs have MixRamp tags (or ``mixramp_analyzer`` is enabled)
- both songs have the same audio format (or :ref:`audio_output_format`
  is configured)

The `MixRamp <http://sourceforge.net/projects/mixramp>`__ tool can be
used to add MixRamp tags to your song files.  To analyze songs
on-the-fly, you can enable the ``mixramp_analyzer`` option in
:file:`mpd.conf`::

 mixramp_analyzer "yes"


Client Connections
------------------

.. _listeners:

Listeners
^^^^^^^^^

The setting :code:`bind_to_address` specifies which addresses
:program:`MPD` listens on for connections from clients.  It can be
used multiple times to bind to more than one address.  Example::

 bind_to_address "192.168.1.42"
 bind_to_address "127.0.0.1"

The default is "any", which binds to all available addresses.
Additionally, MPD binds to :code:`$XDG_RUNTIME_DIR/mpd/socket` (if it
was launched as a per-user daemon and no :code:`bind_to_address`
setting exists).

You can set a port that is different from the global port setting,
e.g. "localhost:6602".  IPv6 addresses must be enclosed in square
brackets if you want to configure a port::

 bind_to_address "[::1]:6602"

To bind to a local socket (UNIX domain socket), specify an absolute
path or a path starting with a tilde (~).  Some clients default to
connecting to :file:`/var/run/mpd/socket` so this may be a good
choice::

 bind_to_address "/var/run/mpd/socket"

On Linux, local sockets can be bound to a name without a socket inode
on the filesystem; MPD implements this by prepending ``@`` to the
address::

 bind_to_address "@mpd"

If no port is specified, the default port is 6600.  This default can
be changed with the port setting::

 port "6601"

These settings will be ignored if `systemd socket activation`_ is
used.


Permissions and Passwords
^^^^^^^^^^^^^^^^^^^^^^^^^

By default, all clients are unauthenticated and have a full set of permissions. This can be restricted with the settings :code:`default_permissions` and :code:`password`.

:code:`default_permissions` controls the permissions of a new client. Its value is a comma-separated list of permissions:

.. list-table::
   :widths: 20 80
   :header-rows: 1

   * - Name
     - Description
   * - **read**
     - Allows reading of the database, displaying the current playlist, and current status of :program:`MPD`.
   * - **add**
     - Allows adding songs and loading playlists.
   * - **player**
     - Allows any player and queue manipulation (start/pause/stop
       playback etc.).
   * - **control**
     - Allows all other player and playlist manipulations.
   * - **admin**
     - Allows manipulating outputs, stickers and partitions,
       mounting/unmounting storage and shutting down :program:`MPD`.

:code:`local_permissions` may be used to assign other permissions to clients connecting on a local socket.

:code:`host_permissions` may be used to assign permissions to clients
with a certain IP address.

:code:`password` allows the client to send a password to gain other permissions. This option may be specified multiple times with different passwords.

Note that the :code:`password` option is not secure: passwords are sent in clear-text over the connection, and the client cannot verify the server's identity.

Example:

.. code-block:: none

    default_permissions "read"
    host_permissions "192.168.0.100 read,add,control,admin"
    host_permissions "2003:1234:4567::1 read,add,control,admin"
    password "the_password@read,add,control"
    password "the_admin_password@read,add,control,admin"

Other Settings
--------------

.. _metadata_to_use:

.. list-table::
   :widths: 20 80
   :header-rows: 1

   * - Setting
     - Description
   * - **metadata_to_use TAG1,TAG2,...**
     - Use only the specified tags, and ignore the others. This
       setting can reduce the database size and :program:`MPD`'s
       memory usage by omitting unused tags. By default, all tags but
       comment are enabled. The special value "none" disables all
       tags.

       If the setting starts with ``+`` or ``-``, then the following
       tags will be added or remoted to/from the current set of tags.
       This example just enables the "comment" tag without disabling all
       the other supported tags

         metadata_to_use "+comment"

       Section :ref:`tags` contains a list of supported tags.

The State File
^^^^^^^^^^^^^^

 The state file is a file where :program:`MPD` saves and restores its state (play queue, playback position etc.) to keep it persistent across restarts and reboots. It is an optional setting.

:program:`MPD` will attempt to load the state file during startup, and will save it when shutting down the daemon. Additionally, the state file is refreshed every two minutes (after each state change).

.. list-table::
   :widths: 20 80
   :header-rows: 1

   * - Setting
     - Description
   * - **state_file PATH**
     - Specify the state file location. The parent directory must be writable by the :program:`MPD` user (+wx).
   * - **state_file_interval SECONDS**
     - Auto-save the state file this number of seconds after each state change. Defaults to 120 (2 minutes).
   * - **restore_paused yes|no**
     - If set to :samp:`yes`, then :program:`MPD` is put into pause mode instead of starting playback after startup. Default is :samp:`no`.

The Sticker Database
^^^^^^^^^^^^^^^^^^^^

"Stickers" are pieces of information attached to songs. Some clients
use them to store ratings and other volatile data. This feature
requires :program:`SQLite`, compile-time configure option
:code:`-Dsqlite=...`.

.. list-table::
   :widths: 20 80
   :header-rows: 1

   * - Setting
     - Description
   * - **sticker_file PATH**
     - The location of the sticker database.

Resource Limitations
^^^^^^^^^^^^^^^^^^^^

These settings are various limitations to prevent :program:`MPD` from using too many resources (denial of service).

.. list-table::
   :widths: 20 80
   :header-rows: 1

   * - Setting
     - Description
   * - **connection_timeout SECONDS**
     - If a client does not send any new data in this time period, the connection is closed. Clients waiting in "idle" mode are excluded from this. Default is 60.
   * - **max_connections NUMBER**
     - This specifies the maximum number of clients that can be connected to :program:`MPD` at the same time. Default is 100.
   * - **max_playlist_length NUMBER**
     - The maximum number of songs that can be in the playlist. Default is 16384.
   * - **max_command_list_size KBYTES**
     - The maximum size a command list. Default is 2048 (2 MiB).
   * - **max_output_buffer_size KBYTES**
     - The maximum size of the output buffer to a client (maximum response size). Default is 8192 (8 MiB).

Buffer Settings
^^^^^^^^^^^^^^^

Do not change these unless you know what you are doing.

.. list-table::
   :widths: 20 80
   :header-rows: 1

   * - Setting
     - Description
   * - **audio_buffer_size SIZE**
     - Adjust the size of the internal audio buffer. Default is
       :samp:`4 MB` (4 MiB).

Zeroconf
^^^^^^^^

If Zeroconf support (`Avahi <http://avahi.org/>`_ or Apple's Bonjour)
was enabled at compile time with :code:`-Dzeroconf=...`,
:program:`MPD` can announce its presence on the network. The following
settings control this feature:

.. list-table::
   :widths: 20 80
   :header-rows: 1

   * - Setting
     - Description
   * - **zeroconf_enabled yes|no**
     - Enables or disables this feature. Default is yes.
   * - **zeroconf_name NAME**
     - The service name to publish via Zeroconf. The default is "Music Player @ %h".
       %h will be replaced with the hostname of the machine running :program:`MPD`.

Advanced configuration
**********************

.. _satellite:

Satellite setup
---------------

:program:`MPD` runs well on weak machines such as the Raspberry Pi. However, such hardware tends to not have storage big enough to hold a music collection. Mounting music from a file server can be very slow, especially when updating the database.

One approach for optimization is running :program:`MPD` on the file server, which not only exports raw files, but also provides access to a readily scanned database. Example configuration:

.. code-block:: none

    music_directory "nfs://fileserver.local/srv/mp3"
    #music_directory "smb://fileserver.local/mp3"

    database {
        plugin "proxy"
        host "fileserver.local"
    }
      
The :code:`music_directory` setting tells :program:`MPD` to read files from the given NFS server. It does this by connecting to the server from userspace. This does not actually mount the file server into the kernel's virtual file system, and thus requires no kernel cooperation and no special privileges. It does not even require a kernel with NFS support, only the nfs storage plugin (using the libnfs userspace library). The same can be done with SMB/CIFS using the smbclient storage plugin (using libsmbclient).

The database setting tells :program:`MPD` to pass all database queries on to the :program:`MPD` instance running on the file server (using the proxy plugin).

.. _realtime:

Real-Time Scheduling
--------------------

On Linux, :program:`MPD` attempts to configure real-time scheduling for some threads that benefit from it.

This is only possible if you allow :program:`MPD` to do it. This privilege is controlled by :envvar:`RLIMIT_RTPRIO` :envvar:`RLIMIT_RTTIME`. You can configure this privilege with :command:`ulimit` before launching :program:`MPD`:

.. code-block:: none

    ulimit -HS -r 40; mpd

Or you can use the :command:`prlimit` program from the util-linux package:

.. code-block:: none

    prlimit --rtprio=40 --rttime=unlimited mpd

The systemd service file shipped with :program:`MPD` comes with this setting.

This works only if the Linux kernel was compiled with :makevar:`CONFIG_RT_GROUP_SCHED` disabled. Use the following command to check this option for your current kernel:

.. code-block:: none

    zgrep ^CONFIG_RT_GROUP_SCHED /proc/config.gz

You can verify whether the real-time scheduler is active with the ps command:

.. code-block:: none

    # ps H -q `pidof -s mpd` -o 'pid,tid,cls,rtprio,comm'
      PID   TID CLS RTPRIO COMMAND
    16257 16257  TS      - mpd
    16257 16258  TS      - io
    16257 16259  FF     40 rtio
    16257 16260  TS      - player
    16257 16261  TS      - decoder
    16257 16262  FF     40 output:ALSA
    16257 16263 IDL      0 update

The CLS column shows the CPU scheduler; TS is the normal scheduler; FF and RR are real-time schedulers. In this example, two threads use the real-time scheduler: the output thread and the rtio (real-time I/O) thread; these two are the important ones. The database update thread uses the idle scheduler ("IDL in ps), which only gets CPU when no other process needs it.

.. note::

   There is a rumor that real-time scheduling improves audio
   quality. That is not true. All it does is reduce the probability of
   skipping (audio buffer xruns) when the computer is under heavy
   load.

Using MPD
*********

Starting and Stopping MPD
-------------------------

The simplest (but not the best) way to start :program:`MPD` is to
simply type::

 mpd

This will start :program:`MPD` as a daemon process (which means it
detaches from your terminal and continues to run in background).  To
stop it, send ``SIGTERM`` to the process; if you have configured a
``pid_file``, you can use the ``--kill`` option::

 mpd --kill

The best way to manage :program:`MPD` processes is to use a service
manager such as :program:`systemd`.

systemd
^^^^^^^

:program:`MPD` ships with :program:`systemd` service units.

If you have installed :program:`MPD` with your operating system's
package manager, these are probably preinstalled, so you can start and
stop :program:`MPD` this way (like any other service)::

 systemctl start mpd
 systemctl stop mpd

systemd socket activation
^^^^^^^^^^^^^^^^^^^^^^^^^

Using systemd, you can launch :program:`MPD` on demand when the first client attempts to connect.

:program:`MPD` comes with two systemd unit files: a "service" unit and
a "socket" unit.  These will be installed to the directory specified
with :code:`-Dsystemd_system_unit_dir=...`,
e.g. :file:`/lib/systemd/system`.

To enable socket activation, type:

.. code-block:: none

    systemctl enable mpd.socket
    systemctl start mpd.socket

In this configuration, :program:`MPD` will ignore the :ref:`listener
settings <listeners>` (``bind_to_address`` and ``port``).

systemd user unit
^^^^^^^^^^^^^^^^^

You can launch :program:`MPD` as a systemd user unit.  These will be
installed to the directory specified with
:code:`-Dsystemd_user_unit_dir=...`,
e.g. :file:`/usr/lib/systemd/user` or
:file:`$HOME/.local/share/systemd/user`.

Once the user unit is installed, you can start and stop :program:`MPD` like any other service:

.. code-block:: none

    systemctl --user start mpd

To auto-start :program:`MPD` upon login, type:

.. code-block:: none

    systemctl --user enable mpd

.. _signals:

Signals
-------

:program:`MPD` understands the following UNIX signals:

- ``SIGTERM``, ``SIGINT``: shut down MPD
- ``SIGHUP``: reopen log files (send this after log rotation) and
  flush caches (see :ref:`input_cache`)


The client
----------

After you have installed, configured and started :program:`MPD`, you choose a client to control the playback.

The most basic client is :program:`mpc`, which provides a command line interface. It is useful in shell scripts. Many people bind specific :program:`mpc` commands to hotkeys.

The `MPD Wiki <http://www.musicpd.org/clients/>`_ contains an extensive list of clients to choose from.

The music directory and the database
------------------------------------

The "music directory" is where you store your music files. :program:`MPD` stores all relevant meta information about all songs in its "database". Whenever you add, modify or remove songs in the music directory, you have to update the database, for example with mpc:

.. code-block:: none

    mpc update

Depending on the size of your music collection and the speed of the storage, this can take a while.

To exclude a file from the update, create a file called :file:`.mpdignore` in its parent directory. Each line of that file may contain a list of shell wildcards. Matching files in the current directory and all subdirectories are excluded.

Mounting other storages into the music directory
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

:program:`MPD` has various storage plugins of which multiple instances can be "mounted" into the music directory. This way, you can use local music, file servers and USB sticks at the same time. Example:

.. code-block:: none

    mpc mount foo nfs://192.168.1.4/export/mp3
    mpc mount usbstick udisks://by-uuid-2F2B-D136
    mpc unmount usbstick

:program:`MPD`'s neighbor plugins can be helpful with finding mountable storages:

.. code-block:: none

    mpc listneighbors

Mounting is only possible with the simple database plugin and a :code:`cache_directory`, e.g.:

.. code-block:: none

    database {
      plugin "simple"
      path "~/.mpd/db"
      cache_directory "~/.mpd/cache"
    }
        
This requires migrating from the old :code:`db_file` setting to a database section. The cache directory must exist, and :program:`MPD` will put one file per mount there, which will be reused when the same storage is used again later.

Metadata
--------

When scanning or playing a song, :program:`MPD` parses its metadata.
See :ref:`tags` for a list of supported tags.

The :ref:`metadata_to_use <metadata_to_use>` setting can be used to
enable or disable certain tags.

Note that :program:`MPD` may not necessarily read metadata itself,
instead relying on data reported by the decoder that was used to read
a file. For example, this is the case for the FFmpeg decoder: both
:program:`MPD` and FFmpeg need to support a given metadata format in
order for metadata to be picked up correctly.

Only if a decoder does not have metadata support will :program:`MPD`
attempt to parse a song's metadata itself.

The queue
---------

The queue (sometimes called "current playlist") is a list of songs to be played by :program:`MPD`. To play a song, add it to the queue and start playback. Most clients offer an interface to edit the queue.

.. _stored_playlists:

Stored Playlists
----------------

Stored playlists are some kind of secondary playlists which can be
created, saved, edited and deleted by the client. They are addressed
by their names.  Its contents can be loaded into the queue, to be
played back.  The :code:`playlist_directory` setting specifies where
those playlists are stored.

Advanced usage
**************

Bit-perfect playback
--------------------

"Bit-perfect playback" is a phrase used by audiophiles to describe a setup that plays back digital music as-is, without applying any modifications such as resampling, format conversion or software volume. Naturally, this implies a lossless codec.

By default, :program:`MPD` attempts to do bit-perfect playback, unless you tell it not to. Precondition is a sound chip that supports the audio format of your music files. If the audio format is not supported, :program:`MPD` attempts to fall back to the nearest supported audio format, trying to lose as little quality as possible.

To verify if :program:`MPD` converts the audio format, enable verbose logging, and watch for these lines:

.. code-block:: none

    decoder: audio_format=44100:24:2, seekable=true
    output: opened plugin=alsa name="An ALSA output" audio_format=44100:16:2
    output: converting from 44100:24:2

This example shows that a 24 bit file is being played, but the sound chip cannot play 24 bit. It falls back to 16 bit, discarding 8 bit.

However, this does not yet prove bit-perfect playback; ALSA may be fooling :program:`MPD` that the audio format is supported. To verify the format really being sent to the physical sound chip, try:

.. code-block:: none

    cat /proc/asound/card*/pcm*p/sub*/hw_params
    access: RW_INTERLEAVED
    format: S16_LE
    subformat: STD
    channels: 2
    rate: 44100 (44100/1)
    period_size: 4096
    buffer_size: 16384

Obey the "format" row, which indicates that the current playback format is 16 bit (signed 16 bit integer, little endian).

Check list for bit-perfect playback:

* Use the ALSA output plugin.
* Disable sound processing inside ALSA by configuring a "hardware"
  device (:samp:`hw:0,0` or similar).
* Don't use software volume (setting :code:`mixer_type`).
* Don't use :ref:`replay_gain`.
* Don't force :program:`MPD` to use a specific audio format (settings
  :code:`format`, :ref:`audio_output_format <audio_output_format>`).
* Verify that you are really doing bit-perfect playback using :program:`MPD`'s verbose log and :file:`/proc/asound/card*/pcm*p/sub*/hw_params`. Some DACs can also indicate the audio format.

.. _dsd:

Direct Stream Digital (DSD)
---------------------------

DSD (`Direct Stream Digital
<https://en.wikipedia.org/wiki/Direct_Stream_Digital>`_) is a digital
format that stores audio as a sequence of single-bit values at a very
high sampling rate.  It is the sample format used on `Super Audio CDs
<https://en.wikipedia.org/wiki/Super_Audio_CD>`_.

:program:`MPD` understands the file formats :ref:`DSDIFF
<decoder_dsdiff>` and :ref:`DSF <decoder_dsf>`.  There are three ways
to play back DSD:

* Native DSD playback. Requires ALSA 1.0.27.1 or later, a sound driver/chip that supports DSD and of course a DAC that supports DSD.

* DoP (DSD over PCM) playback. This wraps DSD inside fake 24 bit PCM according to the DoP standard. Requires a DAC that supports DSD. No support from ALSA and the sound chip required (except for bit-perfect 24 bit PCM support).
* Convert DSD to PCM on-the-fly. 

Native DSD playback is used automatically if available. DoP is only
used if enabled explicitly using the :code:`dop` option, because there
is no way for :program:`MPD` to find out whether the DAC supports
it. DSD to PCM conversion is the fallback if DSD cannot be used
directly.

ICY-MetaData
------------

Some MP3 streams send information about the current song with a
protocol named `"ICY-MetaData"
<http://www.smackfu.com/stuff/programming/shoutcast.html>`_.
:program:`MPD` makes its ``StreamTitle`` value available as ``Title``
tag.

By default, :program:`MPD` assumes this tag is UTF-8-encoded.  To tell
:program:`MPD` to assume a different character set, specify it in the
``charset`` URL fragment parameter, e.g.::

 mpc add 'http://radio.example.com/stream#charset=cp1251'


Client Hacks
************

.. _external_mixer:

External Mixer
--------------

The setting :code:`mixer_type "null"` asks MPD to pretend that there is a mixer, but not actually do something. This allows you to implement a :program:`MPD` client which listens for mixer events, queries the current (fake) volume, and uses it to program an external mixer. For example, your client can forward this setting to your amplifier.

Troubleshooting
***************

Where to start
--------------

Make sure you have the latest :program:`MPD` version (via :code:`mpd --version`, not mpc version). All the time, bugs are found and fixed, and your problem might be a bug that is fixed already. Do not ask for help unless you have the latest :program:`MPD` version. The most common excuse is when your distribution ships an old :program:`MPD` version - in that case, please ask your distribution for help, and not the :program:`MPD` project.

Check the log file. Configure :code:`log_level "verbose"` or pass :option:`--verbose` to mpd.

Sometimes, it is helpful to run :program:`MPD` in a terminal and follow what happens. This is how to do it:

.. code-block:: none

    mpd --stderr --no-daemon --verbose

Support
-------

Getting Help
^^^^^^^^^^^^

The :program:`MPD` project runs a `forum <https://forum.musicpd.org/>`_ and an IRC channel (#mpd on Libera.Chat) for requesting help. Visit the MPD help page for details on how to get help.

Common Problems
^^^^^^^^^^^^^^^

Startup
"""""""

Error "could not get realtime scheduling"
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

See :ref:`realtime`.  You can safely ignore this, but you won't
benefit from real-time scheduling.  This only makes a difference if
your computer runs programs other than MPD.

Error "Failed to initialize io_uring"
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Linux specific: the io_uring subsystem could not be initialized.  This
is not a critical error - MPD will fall back to "classic" blocking
disk I/O.  You can safely ignore this error, but you won't benefit
from io_uring's advantages.

* "Cannot allocate memory" usually means that your memlock limit
  (``ulimit -l`` in bash or ``LimitMEMLOCK`` in systemd) is too low.
  64 MB is a reasonable value for this limit.
* Your Linux kernel might be too old and does not support io_uring.

Error "bind to '0.0.0.0:6600' failed (continuing anyway, because binding to '[::]:6600' succeeded)"
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This happens on Linux when :file:`/proc/sys/net/ipv6/bindv6only` is
disabled.  MPD first binds to IPv6, and this automatically binds to
IPv4 as well; after that, MPD binds to IPv4, but that fails.  You can
safely ignore this, because MPD works on both IPv4 and IPv6.


Database
""""""""

I can't see my music in the MPD database
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

* Check your :code:`music_directory` setting. 
* Does the MPD user have read permission on all music files, and read+execute permission on all music directories (and all of their parent directories)? 
* Did you update the database? (mpc update) 
* Did you enable all relevant decoder plugins at compile time? :command:`mpd --version` will tell you. 

MPD doesn't read ID3 tags!
~~~~~~~~~~~~~~~~~~~~~~~~~~

* You probably compiled :program:`MPD` without libid3tag. :command:`mpd --version` will tell you.

Playback
""""""""

I can't hear music on my client
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

* That problem usually follows a misunderstanding of the nature of :program:`MPD`. :program:`MPD` is a remote-controlled music player, not a music distribution system. Usually, the speakers are connected to the box where :program:`MPD` runs, and the :program:`MPD` client only sends control commands, but the client does not actually play your music.

  :program:`MPD` has output plugins which allow hearing music on a remote host (such as httpd), but that is not :program:`MPD`'s primary design goal. 

Error "Device or resource busy"
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

*  This ALSA error means that another program uses your sound hardware exclusively. You can stop that program to allow :program:`MPD` to use it.

  Sometimes, this other program is PulseAudio, which can multiplex sound from several applications, to allow them to share your sound chip. In this case, it might be a good idea for :program:`MPD` to use PulseAudio as well, instead of using ALSA directly.

Reporting Bugs
--------------

If you believe you found a bug in :program:`MPD`, report it on the `bug tracker <https://github.com/MusicPlayerDaemon/MPD/issues>`_.

Your bug report should contain:

* the output of :command:`mpd --version`
* your configuration file (:file:`mpd.conf`)
* relevant portions of the log file (:option:`--verbose`)
* be clear about what you expect MPD to do, and what is actually happening

.. _profiler:

Too Much CPU Usage
^^^^^^^^^^^^^^^^^^

If you believe MPD consumes too much CPU, `write a bug report
<https://github.com/MusicPlayerDaemon/MPD/issues>`_ with a profiling
information.

On Linux, this can be obtained with :program:`perf` (on Debian,
installed the package :file:`linux-perf`), for example::

 perf record -p `pidof mpd`

Run this command while MPD consumes much CPU, let it run for a minute
or so, and stop it by pressing ``Ctrl-C``.  Then type::

 perf report >mpd_perf.txt

Upload the output file to the bug report.

.. note::

   This requires having debug symbols for MPD and all relevant
   libraries.  See :ref:`crash` for details.

.. _crash:

MPD crashes
^^^^^^^^^^^

All :program:`MPD` crashes are bugs which must be fixed by a developer, and you should write a bug report. (Many crash bugs are caused by codec libraries used by :program:`MPD`, and then that library must be fixed; but in any case, the :program:`MPD` `bug tracker <https://github.com/MusicPlayerDaemon/MPD/issues>`_ is a good place to report it first if you don't know.)

A crash bug report needs to contain a "backtrace".

First of all, your :program:`MPD` executable must not be "stripped"
(i.e. debug information deleted).  The executables shipped with Linux
distributions are usually stripped, but some have so-called "debug"
packages (package :file:`mpd-dbgsym` or :file:`mpd-dbg` on Debian,
:file:`mpd-debug` on other distributions).  Make sure this package is
installed.

If you built :program:`MPD` from sources, please recompile with Meson
option ":code:`--buildtype=debug -Db_ndebug=false`", because this will
add more helpful information to the backtrace.

You can extract the backtrace from a core dump, or by running :program:`MPD` in a debugger, e.g.:

.. code-block:: none

    gdb --args mpd --stderr --no-daemon --verbose
    run

As soon as you have reproduced the crash, type ":command:`bt`" on the
gdb command prompt. Copy the output to your bug report.
