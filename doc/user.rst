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

Install the package :program:`MPD` via :program:`APT`:


.. code-block:: none

    apt-get install mpd

When installed this way, :program:`MPD` by default looks for music in :file:`/var/lib/mpd/music/`; this may not be correct. Look at your :file:`/etc/mpd.conf` file... 

Installing on Android
---------------------

An experimental Android build is available on Google Play. After installing and launching it, :program:`MPD` will scan the music in your Music directory and you can control it as usual with a :program:`MPD` client.

If you need to tweak the configuration, you can create a file called :file:`mpd.conf` on the data partition (the directory which is returned by Android's :dfn:`getExternalStorageDirectory()` API function). 

ALSA is not available on Android; only the :ref:`OpenSL ES
<sles_output>` output plugin can be used for local playback.

Compiling from source
---------------------

Download the source tarball from the `MPD home page <https://musicpd.org>`_ and unpack it:

.. code-block:: none

    tar xf mpd-version.tar.xz
    cd mpd-version

In any case, you need:

* a C++14 compiler (e.g. gcc 6.0 or clang 3.9)
* `Meson 0.47 <http://mesonbuild.com/>`__ and `Ninja
  <https://ninja-build.org/>`__
* Boost 1.58
* pkg-config 

Each plugin usually needs a codec library, which you also need to install. Check the plugin reference for details about required libraries :ref:`plugin_references`.

For example, the following installs a fairly complete list of build dependencies on Debian Jessie:

.. code-block:: none

    apt-get install g++ \
      libmad0-dev libmpg123-dev libid3tag0-dev \
      libflac-dev libvorbis-dev libopus-dev \
      libadplug-dev libaudiofile-dev libsndfile1-dev libfaad-dev \
      libfluidsynth-dev libgme-dev libmikmod2-dev libmodplug-dev \
      libmpcdec-dev libwavpack-dev libwildmidi-dev \
      libsidplay2-dev libsidutils-dev libresid-builder-dev \
      libavcodec-dev libavformat-dev \
      libmp3lame-dev libtwolame-dev libshine-dev \
      libsamplerate0-dev libsoxr-dev \
      libbz2-dev libcdio-paranoia-dev libiso9660-dev libmms-dev \
      libzzip-dev \
      libcurl4-gnutls-dev libyajl-dev libexpat-dev \
      libasound2-dev libao-dev libjack-jackd2-dev libopenal-dev \
      libpulse-dev libroar-dev libshout3-dev \
      libsndio-dev \
      libmpdclient-dev \
      libnfs-dev libsmbclient-dev \
      libupnp-dev \
      libavahi-client-dev \
      libsqlite3-dev \
      libsystemd-dev libwrap0-dev \
      libgtest-dev \
      libboost-dev \
      libicu-dev
      

Now configure the source tree:

.. code-block:: none

 meson . output/release --buildtype=debugoptimized -Db_ndebug=true

The following command shows a list of compile-time options:

.. code-block:: none

 meson configure output/release

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

Just like with the native build, unpack the :program:`MPD` source
tarball and change into the directory.  Then, instead of
:program:`meson`, type:

.. code-block:: none

 mkdir -p output/win64
 cd output/win64
 ../../win32/build.py --64

This downloads various library sources, and then configures and builds :program:`MPD` (for x64; to build a 32 bit binary, pass :option:`--32`). The resulting EXE files is linked statically, i.e. it contains all the libraries already and you do not need carry DLLs around. It is large, but easy to use. If you wish to have a small mpd.exe with DLLs, you need to compile manually, without the :file:`build.py` script.

Compiling for Android
---------------------

:program:`MPD` can be compiled as an Android app. It can be installed easily with Google Play, but if you want to build it from source, follow this section.

You need:

* Android SDK
* Android NDK 

Just like with the native build, unpack the :program:`MPD` source
tarball and change into the directory.  Then, instead of
:program:`meson`, type:

.. code-block:: none

 mkdir -p output/android
 cd output/android
 ../../android/build.py SDK_PATH NDK_PATH ABI
 meson configure -Dandroid_debug_keystore=$HOME/.android/debug.keystore
 ninja android/apk/mpd-debug.apk

:envvar:`SDK_PATH` is the absolute path where you installed the Android SDK; :envvar:`NDK_PATH` is the Android NDK installation path; ABI is the Android ABI to be built, e.g. "armeabi-v7a".

This downloads various library sources, and then configures and builds :program:`MPD`. 

systemd socket activation
-------------------------

Using systemd, you can launch :program:`MPD` on demand when the first client attempts to connect.

:program:`MPD` comes with two systemd unit files: a "service" unit and
a "socket" unit.  These will be installed to the directory specified
with :option:`-Dsystemd_system_unit_dir=...`,
e.g. :file:`/lib/systemd/system`.

To enable socket activation, type:

.. code-block:: none

    systemctl enable mpd.socket
    systemctl start mpd.socket

In this configuration, :program:`MPD` will ignore the :ref:`listener
settings <listeners>` (``bind_to_address`` and ``port``).

systemd user unit
-----------------

You can launch :program:`MPD` as a systemd user unit.  These will be
installed to the directory specified with
:option:`-Dsystemd_user_unit_dir=...`,
e.g. :file:`/usr/lib/systemd/user` or
:file:`$HOME/.local/share/systemd/user`.

Once the user unit is installed, you can start and stop :program:`MPD` like any other service:

.. code-block:: none

    systemctl --user start mpd

To auto-start :program:`MPD` upon login, type:

.. code-block:: none

    systemctl --user enable mpd

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

The include directive can be used to include settings from another file; the given file name is relative to the current file:

.. code-block:: none

  include "other.conf"

Configuring the music directory
-------------------------------

When you play local files, you should organize them within a directory called the "music directory". This is configured in :program:`MPD` with the music_directory setting.

By default, :program:`MPD` follows symbolic links in the music directory. This behavior can be switched off: :code:`follow_outside_symlinks` controls whether :program:`MPD` follows links pointing to files outside of the music directory, and :code:`follow_inside_symlinks` lets you disable symlinks to files inside the music directory.

Instead of using local files, you can use storage plugins to access files on a remote file server. For example, to use music from the SMB/CIFS server "myfileserver" on the share called "Music", configure the music directory "smb://myfileserver/Music". For a recipe, read the Satellite :program:`MPD` section :ref:`satellite`.

You can also use multiple storage plugins to assemble a virtual music directory consisting of multiple storages. 

Configuring database plugins
----------------------------

If a music directory is configured, one database plugin is used. To configure this plugin, add a database block to :file:`mpd.conf`:

.. code-block:: none

    database {
        plugin "simple"
        path "/var/lib/mpd/db"
    }
    
More information can be found in the database plugin reference :ref`database_plugins`. 

Configuring neighbor plugins
----------------------------

All neighbor plugins are disabled by default to avoid unwanted overhead. To enable (and configure) a plugin, add a neighbor block to :file:`mpd.conf`:

.. code-block:: none

    neighbors {
        plugin "smbclient"
    }
      
More information can be found in the neighbor plugin reference :ref:`neighbor_plugin`. 

Configuring input plugins
-------------------------

To configure an input plugin, add a input block to :file:`mpd.conf`:

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

More information can be found in the input plugin reference :ref:`input_plugins`.  

Configuring decoder plugins
---------------------------

Most decoder plugins do not need any special configuration. To configure a decoder, add a decoder block to :file:`mpd.conf`:

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

More information can be found in the decoder plugin reference :ref:`decoder_plugins`. 

Configuring encoder plugins
---------------------------

Encoders are used by some of the output plugins (such as shout). The encoder settings are included in the audio_output section.

More information can be found in the encoder plugin reference :ref:`encoder_plugins`. 

Configuring audio outputs
-------------------------

Audio outputs are devices which actually play the audio chunks produced by :program:`MPD`. You can configure any number of audio output devices, but there must be at least one. If none is configured, :program:`MPD` attempts to auto-detect. Usually, this works quite well with ALSA, OSS and on Mac OS X.

To configure an audio output manually, add one or more audio_output blocks to :file:`mpd.conf`:

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
   * - **format**
     -  Always open the audio output with the specified audio format samplerate:bits:channels), regardless of the format of the input file. This is optional for most plugins.

        Any of the three attributes may be an asterisk to specify that this attribute should not be enforced, example: 48000:16:*. *:*:* is equal to not having a format specification.

        The following values are valid for bits: 8 (signed 8 bit integer samples), 16, 24 (signed 24 bit integer samples padded to 32 bit), 32 (signed 32 bit integer samples), f (32 bit floating point, -1.0 to 1.0), "dsd" means DSD (Direct Stream Digital). For DSD, there are special cases such as "dsd64", which allows you to omit the sample rate (e.g. dsd512:2 for stereo DSD512, i.e. 22.5792 MHz).

        The sample rate is special for DSD: :program:`MPD` counts the number of bytes, not bits. Thus, a DSD "bit" rate of 22.5792 MHz (DSD512) is 2822400 from :program:`MPD`'s point of view (44100*512/8).
   * - **enabed yes|no**
     - Specifies whether this audio output is enabled when :program:`MPD` is started. By default, all audio outputs are enabled. This is just the default setting when there is no state file; with a state file, the previous state is restored.
   * - **tags yes|no**
     - If set to no, then :program:`MPD` will not send tags to this output. This is only useful for output plugins that can receive tags, for example the httpd output plugin.
   * - **always_on yes|no**
     - If set to yes, then :program:`MPD` attempts to keep this audio output always open. This may be useful for streaming servers, when you don't want to disconnect all listeners even when playback is accidentally stopped.
   * - **mixer_type hardware|software|null|none**
     - Specifies which mixer should be used for this audio output: the hardware mixer (available for ALSA :ref:`alsa_plugin`, OSS :ref:`oss_plugin` and PulseAudio :ref:`pulse_plugin`), the software mixer, the "null" mixer (null; allows setting the volume, but with no effect; this can be used as a trick to implement an external mixer :ref:`external_mixer`) or no mixer (none). By default, the hardware mixer is used for devices which support it, and none for the others.

Configuring filters
-------------------

Filters are plugins which modify an audio stream.

To configure a filter, add a filter block to :file:`mpd.conf`:

.. code-block:: none

    filter {
        plugin "volume"
        name "software volume"
    }

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

Configuring playlist plugins
----------------------------

Playlist plugins are used to load remote playlists (protocol commands load, listplaylist and listplaylistinfo). This is not related to :program:`MPD`'s playlist directory.

To configure a playlist plugin, add a playlist_plugin block to :file:`mpd.conf`:

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

More information can be found in the playlist plugin reference :ref:`playlist_plugins`.

Audio Format Settings
---------------------

Global Audio Format
~~~~~~~~~~~~~~~~~~~

The setting audio_output_format forces :program:`MPD` to use one audio format for all outputs. Doing that is usually not a good idea. The values are the same as in format in the audio_output section.

Resampler
~~~~~~~~~

Sometimes, music needs to be resampled before it can be played; for example, CDs use a sample rate of 44,100 Hz while many cheap audio chips can only handle 48,000 Hz. Resampling reduces the quality and consumes a lot of CPU. There are different options, some of them optimized for high quality and others for low CPU usage, but you can't have both at the same time. Often, the resampler is the component that is responsible for most of :program:`MPD`'s CPU usage. Since :program:`MPD` comes with high quality defaults, it may appear that :program:`MPD` consumes more CPU than other software.

Check the resampler plugin reference for a list of resamplers and how to configure them :ref:`resampler_plugins`.

Client Connections
------------------

.. _listeners:

Listeners
~~~~~~~~~

The setting :code:`bind_to_address` specifies which addresses
:program:`MPD` listens on for connections from clients.  It can be
used multiple times to bind to more than one address.  Example::

 bind_to_address "192.168.1.42"
 bind_to_address "127.0.0.1"

The default is "any", which binds to all available addresses.

You can set a port that is different from the global port setting,
e.g. "localhost:6602".  IPv6 addresses must be enclosed in square
brackets if you want to configure a port::

 bind_to_address "[::1]:6602"

To bind to a local socket (UNIX domain socket), specify an absolute
path or a path starting with a tilde (~).  Some clients default to
connecting to :file:`/var/run/mpd/socket` so this may be a good
choice::

 bind_to_address "/var/run/mpd/socket"

If no port is specified, the default port is 6600.  This default can
be changed with the port setting::

 port "6601"

These settings will be ignored if `systemd socket activation`_ is
used.


Permissions and Passwords
~~~~~~~~~~~~~~~~~~~~~~~~~

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
   * - **control**
     - Allows all other player and playlist manipulations.
   * - **admin**
     - Allows database updates and allows shutting down :program:`MPD`.

:code:`local_permissions` may be used to assign other permissions to clients connecting on a local socket.

:code:`password` allows the client to send a password to gain other permissions. This option may be specified multiple times with different passwords.

Note that the :code:`password` option is not secure: passwords are sent in clear-text over the connection, and the client cannot verify the server's identity.

Example:

.. code-block:: none

    default_permissions "read"
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
~~~~~~~~~~~~~~

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

The Sticker Database
~~~~~~~~~~~~~~~~~~~~

"Stickers" are pieces of information attached to songs. Some clients
use them to store ratings and other volatile data. This feature
requires :program:`SQLite`, compile-time configure option
:option:`-Dsqlite`.

.. list-table::
   :widths: 20 80
   :header-rows: 1

   * - Setting
     - Description
   * - **sticker_file PATH**
     - The location of the sticker database.

Resource Limitations
~~~~~~~~~~~~~~~~~~~~

These settings are various limitations to prevent :program:`MPD` from using too many resources (denial of service).

.. list-table::
   :widths: 20 80
   :header-rows: 1

   * - Setting
     - Description
   * - **connection_timeout SECONDS**
     - If a client does not send any new data in this time period, the connection is closed. Clients waiting in "idle" mode are excluded from this. Default is 60.
   * - **max_connections NUMBER**
     - This specifies the maximum number of clients that can be connected to :program:`MPD` at the same time. Default is 5.
   * - **max_playlist_length NUMBER**
     - The maximum number of songs that can be in the playlist. Default is 16384.
   * - **max_command_list_size KBYTES**
     - The maximum size a command list. Default is 2048 (2 MiB).
   * - **max_output_buffer_size KBYTES**
     - The maximum size of the output buffer to a client (maximum response size). Default is 8192 (8 MiB).

Buffer Settings
~~~~~~~~~~~~~~~

Do not change these unless you know what you are doing.

.. list-table::
   :widths: 20 80
   :header-rows: 1

   * - Setting
     - Description
   * - **audio_buffer_size KBYTES**
     - Adjust the size of the internal audio buffer. Default is 4096 (4 MiB).

Zeroconf
~~~~~~~~

If Zeroconf support (`Avahi <http://avahi.org/>`_ or Apple's Bonjour)
was enabled at compile time with :option:`-Dzeroconf=...`,
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
     - The service name to publish via Zeroconf. The default is "Music Player".

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

Real-Time Scheduling
--------------------

On Linux, :program:`MPD` attempts to configure real-time scheduling for some threads that benefit from it.

This is only possible you allow :program:`MPD` to do it. This privilege is controlled by :envvar:`RLIMIT_RTPRIO` :envvar:`RLIMIT_RTTIME`. You can configure this privilege with :command:`ulimit` before launching :program:`MPD`:

.. code-block:: none

    ulimit -HS -r 50; mpd

Or you can use the :command:`prlimit` program from the util-linux package:

.. code-block:: none

    prlimit --rtprio=50 --rttime=unlimited mpd

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
    16257 16259  FF     50 rtio
    16257 16260  TS      - player
    16257 16261  TS      - decoder
    16257 16262  FF     50 output:ALSA
    16257 16263 IDL      0 update

The CLS column shows the CPU scheduler; TS is the normal scheduler; FF and RR are real-time schedulers. In this example, two threads use the real-time scheduler: the output thread and the rtio (real-time I/O) thread; these two are the important ones. The database update thread uses the idle scheduler ("IDL in ps), which only gets CPU when no other process needs it.

Note
~~~~

There is a rumor that real-time scheduling improves audio quality. That is not true. All it does is reduce the probability of skipping (audio buffer xruns) when the computer is under heavy load.

Using MPD
*********

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
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

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

The queue
---------

The queue (sometimes called "current playlist") is a list of songs to be played by :program:`MPD`. To play a song, add it to the queue and start playback. Most clients offer an interface to edit the queue.

Stored Playlists
----------------

Stored playlists are some kind of secondary playlists which can be created, saved, edited and deleted by the client. They are addressed by their names. Its contents can be loaded into the queue, to be played back. The playlist_directory setting specifies where those playlists are stored. 

Advanced usage
**************

Bit-perfect playback
--------------------

"Bit-perfect playback" is a phrase used by audiophiles to describe a setup that plays back digital music as-is, without applying any modifications such as resampling, format conversion or software volume. Naturally, this implies a lossless codec.

By default, :program:`MPD` attempts to do bit-perfect playback, unless you tell it not to. Precondition is a sound chip that supports the audio format of your music files. If the audio format is not supported, :program:`MPD` attempts to fall back to the nearest supported audio format, trying to lose as little quality as possible.

To verify if :program:`MPD` converts the audio format, enable verbose logging, and watch for these lines:

.. code-block:: none

    decoder: audio_format=44100:24:2, seekable=true
    output: opened plugin=alsa name="An ALSA output"audio_format=44100:16:2
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
* Disable sound processing inside ALSA by configuring a "hardware" device (hw:0,0 or similar).
* Don't use software volume (setting mixer_type).
* Don't force :program:`MPD` to use a specific audio format (settings format, audio_output_format).
* Verify that you are really doing bit-perfect playback using :program:`MPD`'s verbose log and :file:`/proc/asound/card*/pcm*p/sub*/hw_params`. Some DACs can also indicate the audio format.

Direct Stream Digital (DSD)
---------------------------

DSD (`Direct Stream Digital <https://en.wikipedia.org/wiki/Direct_Stream_Digital>`_) is a digital format that stores audio as a sequence of single-bit values at a very high sampling rate.

:program:`MPD` understands the file formats dff and dsf. There are three ways to play back DSD:

* Native DSD playback. Requires ALSA 1.0.27.1 or later, a sound driver/chip that supports DSD and of course a DAC that supports DSD.

* DoP (DSD over PCM) playback. This wraps DSD inside fake 24 bit PCM according to the DoP standard. Requires a DAC that supports DSD. No support from ALSA and the sound chip required (except for bit-perfect 24 bit PCM support).
* Convert DSD to PCM on-the-fly. 

Native DSD playback is used automatically if available. DoP is only used if enabled explicitly using the dop option, because there is no way for :program:`MPD` to find out whether the DAC supports it. DSD to PCM conversion is the fallback if DSD cannot be used directly.

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

    mpd --stdout --no-daemon --verbose

Support
-------

Getting Help
~~~~~~~~~~~~

The :program:`MPD` project runs a `forum <https://forum.musicpd.org/>`_ and an IRC channel (#mpd on Freenode) for requesting help. Visit the MPD help page for details on how to get help.

Common Problems
~~~~~~~~~~~~~~~

1. Database
^^^^^^^^^^^

Question: I can't see my music in the MPD database!
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

* Check your :code:`music_directory` setting. 
* Does the MPD user have read permission on all music files, and read+execute permission on all music directories (and all of their parent directories)? 
* Did you update the database? (mpc update) 
* Did you enable all relevant decoder plugins at compile time? :command:`mpd --version` will tell you. 

Question: MPD doesn't read ID3 tags!
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

* You probably compiled :program:`MPD` without libid3tag. :command:`mpd --version` will tell you.

2. Playback
^^^^^^^^^^^

Question: I can't hear music on my client!
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

* That problem usually follows a misunderstanding of the nature of :program:`MPD`. :program:`MPD` is a remote-controlled music player, not a music distribution system. Usually, the speakers are connected to the box where :program:`MPD` runs, and the :program:`MPD` client only sends control commands, but the client does not actually play your music.

  :program:`MPD` has output plugins which allow hearing music on a remote host (such as httpd), but that is not :program:`MPD`'s primary design goal. 

Question: "Device or resource busy"
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

*  This ALSA error means that another program uses your sound hardware exclusively. You can stop that program to allow :program:`MPD` to use it.

  Sometimes, this other program is PulseAudio, which can multiplex sound from several applications, to allow them to share your sound chip. In this case, it might be a good idea for :program:`MPD` to use PulseAudio as well, instead of using ALSA directly.

Reporting Bugs
--------------

If you believe you found a bug in :program:`MPD`, report it on the `bug tracker <https://github.com/MusicPlayerDaemon/MPD/issues>`_.

Your bug report should contain:

* the output of mpd --version
* your configuration file (mpd.conf)
* relevant portions of the log file (--verbose)
* be clear about what you expect MPD to do, and what is actually happening

MPD crashes
~~~~~~~~~~~

All :program:`MPD` crashes are bugs which must be fixed by a developer, and you should write a bug report. (Many crash bugs are caused by codec libraries used by :program:`MPD`, and then that library must be fixed; but in any case, the :program:`MPD` `bug tracker <https://github.com/MusicPlayerDaemon/MPD/issues>`_ is a good place to report it first if you don't know.)

A crash bug report needs to contain a "backtrace".

First of all, your :program:`MPD` executable must not be "stripped" (i.e. debug information deleted). The executables shipped with Linux distributions are usually stripped, but some have so-called "debug" packages (package mpd-dbg or mpd-dbgsym on Debian, mpd-debug on other distributions). Make sure this package is installed.

You can extract the backtrace from a core dump, or by running :program:`MPD` in a debugger, e.g.:

.. code-block:: none

    gdb --args mpd --stdout --no-daemon --verbose
    run

As soon as you have reproduced the crash, type "bt" on the gdb command prompt. Copy the output to your bug report.

.. _plugin_references:

Plugin reference
****************

.. _database_plugins:

Database plugins
----------------

simple
~~~~~~

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

proxy
~~~~~

Provides access to the database of another :program:`MPD` instance using libmpdclient. This is useful when you run mount the music directory via NFS/SMB, and the file server already runs a :program:`MPD` instance. Only the file server needs to update the database.

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
     - Send TCP keepalive packets to the "master" :program:`MPD` instance? This option can help avoid certain firewalls dropping inactive connections, at the expensive of a very small amount of additional network traffic. Disabled by default.

upnp
~~~~

Provides access to UPnP media servers.

Storage plugins
---------------

local
~~~~~

The default plugin which gives :program:`MPD` access to local files. It is used when music_directory refers to a local directory.

curl
~~~~

A WebDAV client using libcurl. It is used when :code:`music_directory` contains a http:// or https:// URI, for example :samp:`https://the.server/dav/`.

smbclient
~~~~~~~~~

Load music files from a SMB/CIFS server. It is used when :code:`music_directory` contains a smb:// URI, for example :samp:`smb://myfileserver/Music`.

nfs
~~~

Load music files from a NFS server. It is used when :code:`music_directory` contains a nfs:// URI according to RFC2224, for example :samp:`nfs://servername/path`.

This plugin uses libnfs, which supports only NFS version 3. Since :program:`MPD` is not allowed to bind to "privileged ports", the NFS server needs to enable the "insecure" setting; example :file:`/etc/exports`:

.. code-block:: none

    /srv/mp3 192.168.1.55(ro,insecure)

Don't fear: "insecure" does not mean that your NFS server is insecure. A few decades ago, people thought the concept of "privileged ports" would make network services "secure", which was a fallacy. The absence of this obsolete "security" measure means little.

udisks
~~~~~~

Mount file systems (e.g. USB sticks or other removable media) using the udisks2 daemon via D-Bus. To obtain a valid udisks2 URI, consult the according neighbor plugin :ref:`neighbor_plugin`.

.. _neighbor_plugin:

Neighbor plugins
----------------

smbclient
~~~~~~~~~

Provides a list of SMB/CIFS servers on the local network.

udisks
~~~~~~
Queries the udisks2 daemon via D-Bus and obtain a list of file systems (e.g. USB sticks or other removable media).

upnp
~~~~

Provides a list of UPnP servers on the local network.

.. _input_plugins:

Input plugins
-------------

alsa
~~~~

Allows :program:`MPD` on Linux to play audio directly from a soundcard using the scheme alsa://. Audio is formatted as 44.1 kHz 16-bit stereo (CD format). Examples:

.. code-block:: none

    mpc add alsa:// plays audio from device hw:0,0

.. code-block:: none

    mpc add alsa://hw:1,0 plays audio from device hw:1,0 cdio_paranoia

cdio_paranoia
~~~~~~~~~~~~~

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
~~~~

Opens remote files or streams over HTTP using libcurl.

Note that unless overridden by the below settings (e.g. by setting them to a blank value), general curl configuration from environment variables such as http_proxy or specified in :file:`~/.curlrc` will be in effect.

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

ffmpeg
~~~~~~

Access to various network protocols implemented by the FFmpeg library: gopher://, rtp://, rtsp://, rtmp://, rtmpt://, rtmps://

file
~~~~

Opens local files

mms
~~~

Plays streams with the MMS protocol using `libmms <https://launchpad.net/libmms>`_.

nfs
~~~

Allows :program:`MPD` to access files on NFSv3 servers without actually mounting them (i.e. in userspace, without help from the kernel's VFS layer). All URIs with the nfs:// scheme are used according to RFC2224. Example:

.. code-block:: none

     mpc add nfs://servername/path/filename.ogg

Note that this usually requires enabling the "insecure" flag in the server's /etc/exports file, because :program:`MPD` cannot bind to so-called "privileged" ports. Don't fear: this will not make your file server insecure; the flag was named in a time long ago when privileged ports were thought to be meaningful for security. By today's standards, NFSv3 is not secure at all, and if you believe it is, you're already doomed.

smbclient
~~~~~~~~~

Allows :program:`MPD` to access files on SMB/CIFS servers (e.g. Samba or Microsoft Windows). All URIs with the smb:// scheme are used. Example:

.. code-block:: none

    mpc add smb://servername/sharename/filename.ogg

qobuz
~~~~~

Play songs from the commercial streaming service Qobuz. It plays URLs in the form qobuz://track/ID, e.g.:

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

tidal
~~~~~

Play songs from the commercial streaming service `Tidal <http://tidal.com/>`_. It plays URLs in the form tidal://track/ID, e.g.:

.. code-block:: none

    mpc add tidal://track/59727857

.. list-table::
   :widths: 20 80
   :header-rows: 1

   * - Setting
     - Description
   * - **token TOKEN**
     - The Tidal application token. Since Tidal is unwilling to assign a token to MPD, this needs to be reverse-engineered from another (approved) Tidal client.
   * - **username USERNAME**
     - The Tidal user name.
   * - **password PASSWORD**
     - The Tidal password.
   * - **audioquality Q**
     - The Tidal "audioquality" parameter. Possible values: HI_RES, LOSSLESS, HIGH, LOW. Default is HIGH.

.. _decoder_plugins:
     
Decoder plugins
---------------

adplug
~~~~~~

Decodes AdLib files using libadplug.

.. list-table::
   :widths: 20 80
   :header-rows: 1

   * - Setting
     - Description
   * - **sample_rate**
     - The sample rate that shall be synthesized by the plugin. Defaults to 48000.

audiofile
~~~~~~~~~

Decodes WAV and AIFF files using libaudiofile.

faad
~~~~

Decodes AAC files using libfaad.

ffmpeg
~~~~~~

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
~~~~

Decodes FLAC files using libFLAC.

dsdiff
~~~~~~

Decodes DFF files containing DSDIFF data (e.g. SACD rips).

.. list-table::
   :widths: 20 80
   :header-rows: 1

   * - Setting
     - Description
   * - **lsbitfirst yes|no**
     - Decode the least significant bit first. Default is no.

dsf
~~~

Decodes DSF files containing DSDIFF data (e.g. SACD rips).

fluidsynth
~~~~~~~~~~

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
~~~

Video game music file emulator based on `game-music-emu <https://bitbucket.org/mpyne/game-music-emu/wiki/Home>`_.

.. list-table::
   :widths: 20 80
   :header-rows: 1

   * - Setting
     - Description
   * - **accuracy yes|no**
     - Enable more accurate sound emulation.

hybrid_dsd
~~~~~~~~~~

`Hybrid-DSD <http://dsdmaster.blogspot.de/p/bitperfect-introduces-hybrid-dsd-file.html>`_ is a MP4 container file (*.m4a) which contains both ALAC and DSD data. It is disabled by default, and works only if you explicitly enable it. Without this plugin, the ALAC parts gets handled by the `FFmpeg decoder plugin <https://www.musicpd.org/doc/user/decoder_plugins.html#ffmpeg_decoder>`_. This plugin should be enabled only if you have a bit-perfect playback path to a DSD-capable DAC; for everybody else, playing back the ALAC copy of the file is better.

.. list-table::
   :widths: 20 80
   :header-rows: 1

   * - Setting
     - Description
   * - **gapless yes|no**
     - This specifies whether to support gapless playback of MP3s which have the necessary headers. Useful if your MP3s have headers with incorrect information. If you have such MP3s, it is highly recommended that you fix them using `vbrfix <http://www.willwap.co.uk/Programs/vbrfix.php>`_ instead of disabling gapless MP3 playback. The default is to support gapless MP3 playback.

mad
~~~

Decodes MP3 files using `libmad <http://www.underbit.com/products/mad/>`_.

mikmod
~~~~~~

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
~~~~~~~

Module player based on MODPlug.

.. list-table::
   :widths: 20 80
   :header-rows: 1

   * - Setting
     - Description
   * - **loop_count**
     - Number of times to loop the module if it uses backward loops. Default is 0 which prevents looping. -1 loops forever.

mpcdec
~~~~~~

Decodes Musepack files using `libmpcdec <http://www.musepack.net/>`_.

mpg123
~~~~~~

Decodes MP3 files using `libmpg123 <http://www.mpg123.de/>`_.

opus
~~~~

Decodes Opus files using `libopus <http://www.opus-codec.org/>`_.

pcm
~~~

Read raw PCM samples. It understands the "audio/L16" MIME type with parameters "rate" and "channels" according to RFC 2586. It also understands the MPD-specific MIME type "audio/x-mpd-float".

sidplay
~~~~~~~

C64 SID decoder based on `libsidplayfp <https://sourceforge.net/projects/sidplay-residfp/>`_ or `libsidplay2 <https://sourceforge.net/projects/sidplay2/>`_.

.. list-table::
   :widths: 20 80
   :header-rows: 1

   * - Setting
     - Description
   * - **songlength_database PATH**
     - Location of your songlengths file, as distributed with the HVSC. The sidplay plugin checks this for matching MD5 fingerprints. See http://www.hvsc.c64.org/download/C64Music/DOCUMENTS/Songlengths.faq.
   * - **default_songlength SECONDS**
     - This is the default playing time in seconds for songs not in the songlength database, or in case you're not using a database. A value of 0 means play indefinitely.
   * - **filter yes|no**
     - Turns the SID filter emulation on or off.
   * - **kernal**
     - Only libsidplayfp. Roms are not embedded in libsidplayfp - please note https://sourceforge.net/p/sidplay-residfp/news/2013/01/released-libsidplayfp-100beta1/ But some SID tunes require rom images to play. Make C64 rom dumps from your own vintage gear or use rom files from Frodo or VICE emulation software tarballs. Absolute path to kernal rom image file.
   * - **basic**
     - Only libsidplayfp. Absolute path to basic rom image file.

sndfile
~~~~~~~

Decodes WAV and AIFF files using `libsndfile <http://www.mega-nerd.com/libsndfile/>`_.


vorbis
~~~~~~

Decodes Ogg-Vorbis files using `libvorbis <http://www.xiph.org/ogg/vorbis/>`_.

wavpack
~~~~~~~

Decodes WavPack files using `libwavpack <http://www.wavpack.com/>`_.

wildmidi
~~~~~~~~

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
---------------

flac
~~~~
Encodes into `FLAC <https://xiph.org/flac/>`_ (lossless).

.. list-table::
   :widths: 20 80
   :header-rows: 1

   * - Setting
     - Description
   * - **compression**
     - Sets the libFLAC compression level. The levels range from 0 (fastest, least compression) to 8 (slowest, most compression).

lame
~~~~

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
~~~~

Does not encode anything, passes the input PCM data as-is.

shine
~~~~~

Encodes into MP3 using the `Shine <https://github.com/savonet/shine>`_ library.

.. list-table::
   :widths: 20 80
   :header-rows: 1

   * - Setting
     - Description
   * - **bitrate**
     - Sets the bit rate in kilobit per second.

twolame
~~~~~~~

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
~~~~

Encodes into `Ogg Opus <http://www.opus-codec.org/>`_.

.. list-table::
   :widths: 20 80
   :header-rows: 1

   * - Setting
     - Description
   * - **bitrate**
     - Sets the data rate in bit per second. The special value "auto" lets libopus choose a rate (which is the default), and "max" uses the maximum possible data rate.
   * - **complexity**
     - Sets the `Opus complexity <https://wiki.xiph.org/OpusFAQ#What_is_the_complexity_of_Opus.3F>`_.
   * - **signal**
     - Sets the Opus signal type. Valid values are "auto" (the default), "voice" and "music".
   * - **opustags yes|no**
     - Configures how metadata is interleaved into the stream. If set to yes, then metadata is inserted using ogg stream chaining, as specified in :rfc:`7845`. If set to no (the default), then ogg stream chaining is avoided and other output-dependent method is used, if available.

.. _vorbis_plugin:

vorbis
~~~~~~

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
~~~~
Encodes into WAV (lossless).

.. _resampler_plugins:

Resampler plugins
-----------------

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
~~~~~~~~

A resampler built into :program:`MPD`. Its quality is very poor, but its CPU usage is low. This is the fallback if :program:`MPD` was compiled without an external resampler.

libsamplerate
~~~~~~~~~~~~~

A resampler using `libsamplerate <http://www.mega-nerd.com/SRC/>`_ a.k.a. Secret Rabbit Code (SRC).

.. list-table::
   :widths: 20 80
   :header-rows: 1

   * - Name
     - Description
   * - **type**
     - The interpolator type. See below for a list of known types.

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
~~~~

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

Output plugins
--------------

.. _alsa_plugin:

alsa
~~~~

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
~~
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
~~~~~
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
~~~~

The fifo plugin writes raw PCM data to a FIFO (First In, First Out) file. The data can be read by another program.

.. list-table::
   :widths: 20 80
   :header-rows: 1

   * - Setting
     - Description
   * - **path P**
     - This specifies the path of the FIFO to write to. Must be an absolute path. If the path does not exist, it will be created when MPD is started, and removed when MPD is stopped. The FIFO will be created with the same user and group as MPD is running as. Default permissions can be modified by using the builtin shell command umask. If a FIFO already exists at the specified path it will be reused, and will not be removed when MPD is stopped. You can use the "mkfifo" command to create this, and then you may modify the permissions to your liking.

jack
~~~~
The jack plugin connects to a `JACK server <http://jackaudio.org/>`_.

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
   * - **ringbuffer_size NBYTES**
     - Sets the size of the ring buffer for each channel. Do not configure this value unless you know what you're doing.

httpd
~~~~~
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
     - Binds the HTTP server to the specified address (IPv4, IPv6 or UNIX socket). Multiple addresses in parallel are not supported.
   * - **encoder NAME**
     - Chooses an encoder plugin. A list of encoder plugins can be found in the encoder plugin reference :ref:`encoder_plugins`.
   * - **max_clients MC**
     - Sets a limit, number of concurrent clients. When set to 0 no limit will apply.

null
~~~~
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
~~~
The "Open Sound System" plugin is supported on most Unix platforms.

On Linux, OSS has been superseded by ALSA. Use the ALSA output plugin :ref:`alsa_plugin` instead of this one on Linux.

.. list-table::
   :widths: 20 80
   :header-rows: 1

   * - Setting
     - Description
   * - **device PATH**
     - Sets the path of the PCM device. If not specified, then MPD will attempt to open /dev/sound/dsp and /dev/dsp.

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
~~~~~~
The "OpenAL" plugin uses `libopenal <http://kcat.strangesoft.net/openal.html>`_. It is supported on many platforms. Use only if there is no native plugin for your operating system.

.. list-table::
   :widths: 20 80
   :header-rows: 1

   * - Setting
     - Description
   * - **device NAME**
     - Sets the device which should be used. This can be any valid OpenAL device name. If not specified, then libopenal will choose a default device.

osx
~~~
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
~~~~

The pipe plugin starts a program and writes raw PCM data into its standard input.

.. list-table::
   :widths: 20 80
   :header-rows: 1

   * - Setting
     - Description
   * - **command CMD**
     - This command is invoked with the shell.

.. _pulse_plugin:

pulse
~~~~~
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

roar
~~~~
The roar plugin connects to a `RoarAudio <http://roaraudio.keep-cool.org/>`_ server.

.. list-table::
   :widths: 20 80
   :header-rows: 1

   * - Setting
     - Description
   * - **server HOSTNAME**
     - The host name of the RoarAudio server. If not specified, then :program:`MPD` will connect to the default locations.
   * - **role ROLE**
     - The "role" that :program:`MPD` registers itself as in the RoarAudio server. The default is "music".

recorder
~~~~~~~~
The recorder plugin writes the audio played by :program:`MPD` to a file. This may be useful for recording radio streams.

.. list-table::
   :widths: 20 80
   :header-rows: 1

   * - Setting
     - Description
   * - **path P**
     - Write to this file.
   * - **format_path P**
     - An alternative to path which provides a format string referring to tag values. The special tag iso8601 emits the current date and time in `ISO8601 <https://en.wikipedia.org/wiki/ISO_8601>`_ format (UTC). Every time a new song starts or a new tag gets received from a radio station, a new file is opened. If the format does not render a file name, nothing is recorded. A tag name enclosed in percent signs ('%') is replaced with the tag value. Example: :file:`~/.mpd/recorder/%artist% - %title%.ogg`. Square brackets can be used to group a substring. If none of the tags referred in the group can be found, the whole group is omitted. Example: [~/.mpd/recorder/[%artist% - ]%title%.ogg] (this omits the dash when no artist tag exists; if title also doesn't exist, no file is written). The operators "|" (logical "or") and "&" (logical "and") can be used to select portions of the format string depending on the existing tag values. Example: ~/.mpd/recorder/[%title%|%name%].ogg (use the "name" tag if no title exists)
   * - **encoder NAME**
     - Chooses an encoder plugin. A list of encoder plugins can be found in the encoder plugin reference :ref:`encoder_plugins`.


shout
~~~~~
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
   * - **timeout SECONDS**
     - Set the timeout for the shout connection in seconds. Defaults to 2 seconds.
   * - **protocol icecast2|icecast1|shoutcast**
     - Specifies the protocol that wil be used to connect to the server. The default is "icecast2".
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
~~~~

Plugin using the `OpenSL ES <https://www.khronos.org/opensles/>`__
audio API.  Its primary use is local playback on Android, where
:ref:`ALSA <alsa_plugin>` is not available.


solaris
~~~~~~~
The "Solaris" plugin runs only on SUN Solaris, and plays via /dev/audio.

.. list-table::
   :widths: 20 80
   :header-rows: 1

   * - Setting
     - Description
   * - **device PATH**
     - Sets the path of the audio device, defaults to /dev/audio.

.. _playlist_plugins:
     
Playlist plugins
----------------

asx
~~~

Reads .asx playlist files.

cue
~~~
Reads .cue files.

embcue
~~~~~~
Reads CUE sheets from the "CUESHEET" tag of song files.

m3u
~~~
Reads .m3u playlist files.

extm3u
~~~~~~
Reads extended .m3u playlist files.

flac
~~~~
Reads the cuesheet metablock from a FLAC file.

pls
~~~
Reads .pls playlist files.

rss
~~~
Reads music links from .rss files.

soundcloud
~~~~~~~~~~
Download playlist from SoundCloud. It accepts URIs starting with soundcloud://.

.. list-table::
   :widths: 20 80
   :header-rows: 1

   * - Setting
     - Description
   * - **apikey KEY**
     - An API key to access the SoundCloud servers.

xspf
~~~~
Reads XSPF playlist files. 
