# Music Player Daemon

http://www.musicpd.org

A daemon for playing music of various formats.  Music is played through the 
server's audio device.  The daemon stores info about all available music, 
and this info can be easily searched and retrieved.  Player control, info
retrieval, and playlist management can all be managed remotely.

For basic installation instructions
[read the manual](https://www.musicpd.org/doc/user/install.html).

# Cary Audio Branch

This is a branch of Cary Audio product. We use FIFO output plugin. And 
the other program read audio data from FIFO, then dsp and playback it.
"build-arm.sh" and "build-x64.sh" are build scripts for this branch.

# Users

- [Manual](http://www.musicpd.org/doc/user/)
- [Forum](http://forum.musicpd.org/)
- [IRC](irc://chat.freenode.net/#mpd)
- [Bug tracker](https://github.com/MusicPlayerDaemon/MPD/issues/)

# Developers

- [Protocol specification](http://www.musicpd.org/doc/protocol/)
- [Developer manual](http://www.musicpd.org/doc/developer/)

# Legal

MPD is released under the
[GNU General Public License version 2](https://www.gnu.org/licenses/gpl-2.0.txt),
which is distributed in the COPYING file.
