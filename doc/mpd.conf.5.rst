========
mpd.conf
========


DESCRIPTION
------------

:file:`mpd.conf` is the configuration file for :manpage:`mpd(1)`. If
not specified on the command line, MPD first searches for it at
:file:`$XDG_CONFIG_HOME/mpd/mpd.conf` then at :file:`~/.mpdconf` then
at :file:`~/.mpd/mpd.conf` and then in :file:`/etc/mpd.conf`.

Lines beginning with a :samp:`#` character are comments. All other
non-empty lines specify parameters and their values. These lines
contain the parameter name and parameter value (surrounded by double
quotes) separated by whitespace (either tabs or spaces).  For
example::

   parameter "value"

The exception to this rule is the audio_output parameter, which is of
the form::

   audio_output {
   	parameter1 "value"
   	parameter2 "value"
   }


Parameters that take a file or directory as an argument should use absolute paths.

See :file:`docs/mpdconf.example` in the source tarball for an example
configuration file.

This manual is not complete, it lists only the most important options.
Please read the MPD user manual for a complete configuration guide:
http://www.musicpd.org/doc/user/


REQUIRED PARAMETERS
-------------------

db_file <file>
   This specifies where the db file will be stored.

log_file <file>
   This specifies where the log file should be located. The special value "syslog" makes MPD use the local syslog daemon.

OPTIONAL PARAMETERS
-------------------

sticker_file <file>
   The location of the sticker database. This is a database which manages
   dynamic information attached to songs.

pid_file <file>
   This specifies the file to save mpd's process ID in.

music_directory <directory>
   This specifies the directory where music is located. If you do not configure
   this, you can only play streams.

playlist_directory <directory>
   This specifies the directory where saved playlists are stored. If
   you do not configure this, you cannot save playlists.

state_file <file>
   This specifies if a state file is used and where it is located. The state of
   mpd will be saved to this file when mpd is terminated by a TERM signal or by
   the :program:`kill` command. When mpd is restarted, it will read the state file and
   restore the state of mpd (including the playlist).

restore_paused <yes or no>
   Put MPD into pause mode instead of starting playback after startup.

user <username>
   This specifies the user that MPD will run as, if set. MPD should never run
   as root, and you may use this option to make MPD change its user id after
   initialization. Do not use this option if you start MPD as an unprivileged
   user.

port <port>
   This specifies the port that mpd listens on. The default is 6600.

log_level <default, secure, or verbose>
   Suppress all messages below the given threshold.  The following
   log levels are available:

   - :samp:`error`: errors
   - :samp:`warning`: warnings
   - :samp:`notice`: interesting informational messages
   - :samp:`info`: unimportant informational messages
   - :samp:`verbose`: debug messages (for developers and for
     troubleshooting)

   The default is :samp:`notice`.

follow_outside_symlinks <yes or no>
  Control if MPD will follow symbolic links pointing outside the music dir. You
  must recreate the database after changing this option. The default is "yes".

follow_inside_symlinks <yes or no>
  Control if MPD will follow symbolic links pointing inside the music dir,
  potentially adding duplicates to the database. You must recreate the
  database after changing this option. The default is "yes".

zeroconf_enabled <yes or no>
  If yes, and MPD has been compiled with support for Avahi or Bonjour, service
  information will be published with Zeroconf. The default is yes.

zeroconf_name <name>
  If Zeroconf is enabled, this is the service name to publish. This name should
  be unique to your local network, but name collisions will be properly dealt
  with. The default is "Music Player @ %h", where %h will be replaced with the
  hostname of the machine running MPD.

audio_output
  See DESCRIPTION and the various ``AUDIO OUTPUT PARAMETERS`` sections for the
  format of this parameter. Multiple audio_output sections may be specified. If
  no audio_output section is specified, then MPD will scan for a usable audio
  output.

replaygain <off or album or track or auto>
  If specified, mpd will adjust the volume of songs played using ReplayGain
  tags (see http://www.replaygain.org/). Setting this to "album" will
  adjust volume using the album's ReplayGain tags, while setting it to "track"
  will adjust it using the track ReplayGain tags. "auto" uses the track
  ReplayGain tags if random play is activated otherwise the album ReplayGain
  tags. Currently only FLAC, Ogg Vorbis, Musepack, and MP3 (through ID3v2
  ReplayGain tags, not APEv2) are supported.

replaygain_preamp <-15 to 15>
  This is the gain (in dB) applied to songs with ReplayGain tags.

volume_normalization <yes or no>
  If yes, mpd will normalize the volume of songs as they play. The default is
  no.

filesystem_charset <charset>
  This specifies the character set used for the filesystem. A list of supported
  character sets can be obtained by running "iconv -l". The default is
  determined from the locale when the db was originally created.

save_absolute_paths_in_playlists <yes or no>
  This specifies whether relative or absolute paths for song filenames are used
  when saving playlists. The default is "no".

auto_update <yes or no>
  This specifies the whether to support automatic update of music database
  when files are changed in music_directory. The default is to disable
  autoupdate of database.

auto_update_depth <N>
  Limit the depth of the directories being watched, 0 means only watch the
  music directory itself. There is no limit by default.

REQUIRED AUDIO OUTPUT PARAMETERS
--------------------------------

type <type>
  This specifies the audio output type. See the list of supported outputs in
  ``mpd --version`` for possible values.

name <name>
  This specifies a unique name for the audio output.

OPTIONAL AUDIO OUTPUT PARAMETERS
--------------------------------

format <sample_rate:bits:channels>
  This specifies the sample rate, bits per sample, and number of channels of
  audio that is sent to the audio output device. See documentation for the
  ``audio_output_format`` parameter for more details. The default is to use
  whatever audio format is passed to the audio output. Any of the three
  attributes may be an asterisk to specify that this attribute should not be
  enforced

replay_gain_handler <software, mixer or none>
  Specifies how replay gain is applied. The default is "software", which uses
  an internal software volume control. "mixer" uses the configured (hardware)
  mixer control. "none" disables replay gain on this audio output.

mixer_type <hardware, software or none>
  Specifies which mixer should be used for this audio output: the hardware
  mixer (available for ALSA, OSS and PulseAudio), the software mixer or no
  mixer ("none"). By default, the hardware mixer is used for devices which
  support it, and none for the others.

FILES
-----

:file:`~/.mpdconf`
  User configuration file.

:file:`/etc/mpd.conf`
  Global configuration file.

SEE ALSO
--------

:manpage:`mpd(1)`, :manpage:`mpc(1)`
