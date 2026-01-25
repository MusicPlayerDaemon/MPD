.. _manpage_mpdconf:

========
mpd.conf
========


Description
------------

:file:`mpd.conf` is the configuration file for :manpage:`mpd(1)`.

Usually, that is :file:`/etc/mpd.conf`, unless a different path is
specified on the command line.

If you run :program:`MPD` as a user daemon (and not as a system
daemon), the configuration is read from
:file:`$XDG_CONFIG_HOME/mpd/mpd.conf` (usually
:file:`~/.config/mpd/mpd.conf`). On Android, :file:`mpd.conf` will be
loaded from the top-level directory of the data partition.

Each line in the configuration file contains a setting name and its value, e.g.:

:code:`connection_timeout "5"`

Lines starting with ``#`` are treated as comments and ignored.

For settings that specify a file system path, the tilde (``~``) is
expanded to ``$HOME``. In addition, the following path expansions are
supported:

- ``$HOME``
- ``$XDG_CONFIG_HOME``
- ``$XDG_MUSIC_DIR``
- ``$XDG_CACHE_HOME``
- ``$XDG_RUNTIME_DIR``

Example:

.. code-block:: none

    music_directory "~/Music"
    db_file "$XDG_CONFIG_HOME/mpd/database"

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

See :file:`docs/mpdconf.example` in the source tarball for an example
configuration file.

This manual is not complete, it lists only the most important options.
Please read the MPD user manual for a complete configuration guide:
http://www.musicpd.org/doc/user/

Global Settings
---------------

System Settings
^^^^^^^^^^^^^^^

.. confval:: user
   :type: NAME

   This specifies the user that MPD will run as, if set. MPD should never run
   as root, and you may use this option to make MPD change its user id after
   initialization. Do not use this option if you start MPD as an unprivileged
   user.

.. confval:: group
   :type: NAME

   Change to this group.  This discards all default groups and uses
   only this group.  Do not use this setting.

.. confval:: pid_file
   :type: PATH

   This specifies the file to save mpd's process ID in.

.. confval:: log_file
   :type: PATH

   This specifies where the log file should be located. The special value "syslog" makes MPD use the local syslog daemon.

.. confval:: log_level
   :type: LEVEL
   :default: ``notice``

   Suppress all messages below the given threshold.  The following
   log levels are available:

   - :samp:`error`: errors
   - :samp:`warning`: warnings
   - :samp:`notice`: interesting informational messages
   - :samp:`info`: unimportant informational messages
   - :samp:`verbose`: debug messages (for developers and for
     troubleshooting)


Client Settings
^^^^^^^^^^^^^^^

.. confval:: port
   :type: number
   :default: :samp:`6600`

   This specifies the port that mpd listens on.


File Settings
^^^^^^^^^^^^^

.. confval:: db_file
   :type: PATH

   This specifies where the db file will be stored.

.. confval:: sticker_file
   :type: PATH

   The location of the sticker database. This is a database which manages
   dynamic information attached to songs.

.. confval:: music_directory
   :type: PATH

   This specifies the directory where music is located. If you do not configure
   this, you can only play streams.

.. confval:: playlist_directory
   :type: PATH

   This specifies the directory where saved playlists are stored
   (flat, no subdirectories).  If you do not configure this, you
   cannot save playlists.

.. confval:: state_file
   :type: PATH

   This specifies if a state file is used and where it is located. The state of
   mpd will be saved to this file when mpd is terminated by a TERM signal or by
   the :program:`kill` command. When mpd is restarted, it will read the state file and
   restore the state of mpd (including the playlist).

.. confval:: follow_outside_symlinks
   :type: ``yes`` or ``no``
   :default: ``yes``

   Control if MPD will follow symbolic links pointing outside the music dir. You
   must recreate the database after changing this option.

.. confval:: follow_inside_symlinks
   :type: ``yes`` or ``no``
   :default: ``yes``

   Control if MPD will follow symbolic links pointing inside the music dir,
   potentially adding duplicates to the database. You must recreate the
   database after changing this option.

.. confval:: auto_update
   :type: ``yes`` or ``no``
   :default: ``no``

   This specifies the whether to support automatic update of music database
   when files are changed in music_directory.
   (Only implemented on Linux.)

.. confval:: auto_update_depth
   :type: number
   :default: unlimited

   Limit the depth of the directories being watched, 0 means only watch the
   music directory itself.

.. confval:: save_absolute_paths_in_playlists
   :type: ``yes`` or ``no``
   :default: ``no``

   This specifies whether relative or absolute paths for song filenames are used
   when saving playlists.

.. confval:: filesystem_charset
   :type: CHARSET

   This specifies the character set used for the filesystem. A list of supported
   character sets can be obtained by running "iconv -l". The default is
   determined from the locale when the db was originally created.


Player Settings
^^^^^^^^^^^^^^^

.. confval:: restore_paused
   :type: ``yes`` or ``no``
   :default: ``no``

   Put MPD into pause mode instead of starting playback after startup.


Other Settings
^^^^^^^^^^^^^^

.. confval:: zeroconf_enabled
   :type: ``yes`` or ``no``
   :default: ``yes``

   If yes, and MPD has been compiled with support for Avahi or Bonjour, service
   information will be published with Zeroconf.

.. confval:: zeroconf_name
   :type: NAME
   :default: :samp:`Music Player @ %h`

   If Zeroconf is enabled, this is the service name to publish. This
   name should be unique to your local network, but name collisions
   will be properly dealt with.  ``%h`` will be replaced with the
   hostname of the machine running MPD.

.. confval:: audio_output

   See DESCRIPTION and the various ``AUDIO OUTPUT OPTIONS`` sections for the
   format of this block. Multiple audio_output sections may be specified. If
   no audio_output section is specified, then MPD will scan for a usable audio
   output.

Required Audio Output Settings
------------------------------

.. confval:: type
   :type: NAME

   This specifies the audio output type. See the list of supported outputs in
   ``mpd --version`` for possible values.

.. confval:: name
   :type: NAME

   This specifies a unique name for the audio output.

Optional Audio Output Settings
------------------------------

.. confval:: format
   :type: ``sample_rate:bits:channels``

  This specifies the sample rate, bits per sample, and number of channels of
  audio that is sent to the audio output device. See documentation for the
  ``audio_output_format`` option for more details. The default is to use
  whatever audio format is passed to the audio output. Any of the three
  attributes may be an asterisk to specify that this attribute should not be
  enforced

.. confval:: replay_gain_handler
   :type: ``software``, ``mixer`` or ``none``
   :default: ``software``

   Specifies how replay gain is applied. ``software`` uses an internal
   software volume control. ``mixer`` uses the configured (hardware)
   mixer control. ``none`` disables replay gain on this audio output.

.. confval:: mixer_type
   :type:  ``hardware``, ``software`` or ``none``
   :default: ``hardware`` (if supported) or ``none``

   Specifies which mixer should be used for this audio output: the hardware
   mixer (available for ALSA, OSS and PulseAudio), the software mixer or no
   mixer (``none``).

Files
-----

:file:`$XDG_CONFIG_HOME/mpd/mpd.conf`
  User configuration file (usually :file:`~/.config/mpd/mpd.conf`).

:file:`/etc/mpd.conf`
  Global configuration file.

See Also
--------

:manpage:`mpd(1)`, :manpage:`mpc(1)`
