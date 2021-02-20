Client Developer's Manual
#########################

Introduction
************

MPD is a music player without a user interface.  The user interface
will be provided by independent clients, which connect to MPD over
socket connections (TCP or local sockets).

This chapter describes how to develop a client.

Before you develop a new client, consider joining an existing client
project.  There are many clients, but few are mature; we need fewer,
but better clients.

Client Libraries
****************

There are many libraries which help with connecting to MPD.  If you
develop a MPD client, use a library instead of reinventing the wheel.
The MPD website has a list of libraries: https://www.musicpd.org/libs/


Connecting to MPD
*****************

Do not hard-code your client to connect to ``localhost:6600``.
Instead, use the defaults of the client library.  For example, with
:program:`libmpdclient`, don't do::

 c = mpd_connection_new("localhost", 6600, 30000);

Instead, do::

 c = mpd_connection_new(NULL, 0, 0);

This way, the library can choose the best defaults, maybe derived from
environment variables, so all MPD clients use the same settings.

If you need to reimplement those defaults (or if you are developing a
client library), this is a good set of addresses to attempt to connect
to:

- if the environment variable :envvar:`MPD_HOST` is set:
  ``$MPD_HOST:$MPD_PORT`` (:envvar:`MPD_PORT` defaulting to 6600)
- if the environment variable :envvar:`XDG_RUNTIME_DIR` is set:
  ``$XDG_RUNTIME_DIR/mpd/socket``
- :file:`/run/mpd/socket`
- ``localhost:$MPD_PORT`` (:envvar:`MPD_PORT` defaulting to 6600)

Environment Variables
*********************

The following environment variables should be obeyed by all clients
(preferably by the client library):

- :envvar:`MPD_HOST`: the host (or local socket path) to connect to;
  on Linux, this may start with a ``@`` to connect to an abstract
  socket.  To use a password with MPD, set :envvar:`MPD_HOST` to
  ``password@host`` (then abstract socket requires double ``@``:
  ``password@@socket``).
- :envvar:`MPD_PORT`: the port number; defaults to 6600.
- :envvar:`MPD_TIMEOUT`: timeout for connecting to MPD and for waiting
  for MPD's response in seconds.  A good default is 30 seconds.

