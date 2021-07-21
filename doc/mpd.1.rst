===
mpd
===

SYNOPSIS
--------

``mpd`` [options] [CONF_FILE]

DESCRIPTION
------------

MPD is a daemon for playing music. Music is played through the configured audio output(s) (which are generally local, but can be remote). The daemon stores info about all available music, and this info can be easily searched and retrieved. Player control, info retrieval, and playlist management can all be managed remotely.

MPD searches for a config file in ``$XDG_CONFIG_HOME/mpd/mpd.conf``
then ``~/.mpdconf`` then ``~/.mpd/mpd.conf`` then ``/etc/mpd.conf`` or uses ``CONF_FILE``.

Read more about MPD at http://www.musicpd.org/

OPTIONS
-------

.. program:: mpd

.. option:: --help

  Output a brief help message.

.. option:: --kill

  Kill the currently running mpd session. The pid_file parameter must be specified in the config file for this to work.

.. option:: --no-config

  Don't read from the configuration file.

.. option:: --no-daemon

  Don't detach from console.

.. option:: --stderr

  Print messages to stderr.

.. option:: --verbose

  Verbose logging.

.. option:: --version

  Print version information.

FILES
-----

:file:`$XDG_CONFIG_HOME/mpd/mpd.conf`
  User configuration file (usually :file:`~/.config/mpd/mpd.conf`).

:file:`/etc/mpd.conf`
  Global configuration file.

SEE ALSO
--------

:manpage:`mpd.conf(5)`, :manpage:`mpc(1)`

BUGS
----
If you find a bug, please report it at https://github.com/MusicPlayerDaemon/MPD/issues/
