/*
 * Copyright 2003-2022 The Music Player Daemon Project
 * http://www.musicpd.org
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

/**
 * \page vis_out_protocol Visualization Network Protocol
 *
 * See \ref vis_out "RFC: Visualizatoin Output Plugin" for background.
 * 
 *
 * \section vis_out_protocol_timing Timing
 *
 * In order to deliver sound data to the client at the proper time, the protocol
 * needs to take into account:
 *
 * - network latency: the delta between writing the sound data to the socket & its
 *   receipt on the client
 *
 * - player buffering: the player may buffer sound data (mplayer, for instance,
 *   buffers half a second's worth of audio before beginning playback)
 *
 * - render time: the client presumably wishes the current frame to appear on-screen
 *   at the moment the current sound information is ending
 *
 * Throughout, let \e t be "song time" be measured on the server, and T(t) be
 * sound information for song time \e t. Let FPS be the frames-per-second at
 * which the client would like to render.
 *
 * Then, at an interval of 1/FPS seconds, the server needs to write
 *
 \verbatim
       T(t - {buffer time} + {render time} + {one way latency})
 \endverbatim
 *
 * to the client socket. If we denote that time value by tau, then the server
 * should wait min(0, -tau) ms to write the first frame.
 *
 * A few examples will illustrate.
 *
 * \subsection vis_out_protocol_timing_eg_1 Example 1
 *
 * Let the client render time be 4ms and round-trip network latency be
 * 6ms. Assume no player buffering. In order to render a frame corresponding to
 * song time \e t, the client would need, at time \e t - 4 ms, sound information
 * corresponding to time \e t, or T(t). The server would need to \e send that
 * information at time \e t - 7ms (half of one round-trip plus render time).
 *
 * In other words, on the server side at song time \e t, we would need to write
 * T(t + 7ms) to the client socket. If the server writes T(t+7ms) immediately,
 * the client will receive it at \e t + 4ms, take 4ms to render the next frame,
 * and so at \e t + 7ms hence, finish rendering T(t+7).
 *
 * \subsection vis_out_protocol_timing_eg_2 Example 2
 *
 * Imagine we are running the same client on a laptop, connected to an MPD
 * server over the internet, and using mplayer as the player. This gives 500ms
 * of buffer time.  Let us assume the same 4ms render time, but now a 20ms
 * round-trip time.
 *
 * In order to render a frame corresponding to song time \e t, the client would
 * need, at time \e t - 4ms, T(t). This would need to be sent from the server at
 * time \e t - 14ms. We now need to incorporate the client-side buffering,
 * however. Song time \e t will be actually played on the client at \e t + 500ms
 * on the server.
 *
 * In other words, on the server side at song time \e t, we would need to write
 * T(t-486ms) to the client socket.
 *
 * Since the sound won't start on the client for 0.5 sec, it would make no sense
 * to begin writing sound information for 486ms. Let t(0) be the moment the
 * client connects and the player begins buffering. If, at t(0) + 486ms, the
 * server writes T(t(0)), the client will receive it at t(0) + 496ms & complete
 * rendering it at t(0) + 500ms, which is when the client-side player will
 * begin playing song time t(0).
 *
 * \section vis_out_protocol_proto The Protocol
 *
 * \subsection vis_out_protocol_proto_design Design
 *
 * The author is unaware of any existing network protocols in this area, so he
 * designed his own after reveiwing the Shoutcast & Ultravox
 * protocols. Experience with the TLS & 802.11 protocols also informed this
 * design.
 *
 * Design goals include:
 *
 * - client convenience
 *   - this in particular drove the choice to stream updates; everything
 *     needed to simply push the data out is knowable at handshake time,
 *     so why force the client to send a request?
 * - efficiency on the wire
 *   - binary format
 *   - streaming preferred over request/response
 * - future extensibility
 *   - protocol versioning built-in from the start
 * - parsing convenience
 *   - streaming messages come with a few "magic bytes" at the start
 *     to assist clients in "locking on" to the stream & recovering from
 *     corrupted data
 *   - all messages conform to the "type-length-value" (TLV) format
 *     beloved of parser writers
 *
 *
 * \subsection vis_out_protocol_proto_overview Overview
 *
 * The protocol is a combination of request/response as well as streaming. After
 * an initial handshake (client goes first) the server will begin streaming
 * messages to the client; i.e. at the interval the client specified during the
 * initial handshake the server will send FRAME messages containing sound
 * information useful for visualizers. Additionally, METADATA messages will be
 * sent on receipt of notifications from MPD that the song has changed.  The
 * client need not request these messages or does the client need to acknowledge
 * them in any way.
 *
 * The client \e may, at any time after handshake completion, initiate two other
 * exchanges:
 *
 * - PING: the client may want to periodically adjust it's estimate of the
 *   round-trip client-side latency; the server will respond with a PONG.
 *   The client can measure the delta between request & response & update
 *   its estimate.
 *
 * - ADJBUF: the client may periodically want to adjust the "buffer time";
 *   that is, the delta between song time as measured on the server and
 *   the song time to each FRAME & METADATA frame corresponds; the server
 *   will adjust it's internal timers & respond with an ADJBUFACK message.
 *   The server \e may send one more frame at the old setting of tau.
 *
 * Schematically, a conversation looks like this:
 *
 \verbatim
   Client                                                   Server

   desired protocol version
   tau (buffer offset)
   desired sound params     --------- CLIHLO --------->
     damping parameter
     window parameter
     ...

	                        <-------- SRVHLO --------- offered protocol version
                                                                          |
    sequence number   		--------- PING ---------->                    |
							<-------- PONG ----------  sequence number    | tau ms
                                                                          |
                                                                          |
							<------- METADATA-------- artist, title &c    v
							<-------- FRAME --------- samples, spectrum   |
							                          bass/mids/trebs     |
                                                      ...                 | tau ms
                                                                          |
							<------- METADATA-------- artist, title &c    v
							<-------- FRAME --------- samples, spectrum   |
							                          bass/mids/trebs     |
                                                      ...                 |
    sequence number   		--------- PING ---------->                    |
							<-------- PONG ----------  sequence number    | tau ms
                                                                          |
                                                                          |
							<------- METADATA-------- artist, title &c    v
							<-------- FRAME --------- samples, spectrum   |
							                          bass/mids/trebs     |
                                                      ...                 | tau ms
                                                                          |
							<------- METADATA-------- artist, title &c    v
							<-------- FRAME --------- samples, spectrum   |
							                          bass/mids/trebs     |
                                                      ...                 | tau ms
                                                                          |
    tau' (new buffer        -------- ADJBUF -------->                     |
		  offset)           <------ ADJBUFACK -------                     |
                                                                          |
							<------- METADATA-------- artist, title &c    v
							<-------- FRAME --------- samples, spectrum   |
							                          bass/mids/trebs     |
                                                      ...                 | tau' ms
                                                                          |
							<------- METADATA-------- artist, title &c    v
							<-------- FRAME --------- samples, spectrum
							                          bass/mids/trebs
                                                      ...
				                  ....
								(forever)
 \endverbatim
 *
 * There is no formal "close" or "teardown" message; each side simply detects
 * when the other has gone away & treats that as the end of the conversation.
 *
 *
 * \subsection vis_out_protocol_proto_msgs Messages
 *
 * All messages:
 *
 * - integers use network byte order (i.e. big endian)
 * - use TLV format (streaming messages prepend magic bytes)
 * 
 \verbatim

   +-----------------------+-----------------+-----------------------+--------+
   | TYPE (16-bit unsigned)|     LENGTH      |    PAYLOAD            |  CHECK |
   | class | message type  | 16-bits unsigned| LENGTH bytes          | 1 byte |
   |-------+---------------|-----------------|-----------------------+--------+
   | 4 bits|   12 bits     | (max len 65535) | format is msg-specfic |    00  |
   +-----------------------+-----------------+-----------------------+--------+
   
 \endverbatim
 *
 * Notes:
 *
 * - the message type is comprised of two values packed into a u16_t:
 *
 *   - class: (type & f000) >> 12:
 *     - 0: handshake
 *     - 1; control (PING, e.g.)
 *     - 2: streaming (FRAME, e.g.)
 *
 *   - message type: (type & 0ffff) see below for values
 *
 * - the "length" field is the length of the \e payload \e only
 *
 * - the "check" byte is intended as a sanity test & shall always be zero
 *   TODO(sp1ff): replace this with a proper checksum?
 *
 * TODO(sp1ff): define each message
 *
 *
 */


const struct AudioOutputPlugin visualization_output_plugin = {
	"visualization",
	nullptr, // cannot serve as the default output
	nullptr, // TODO(sp1ff): Write me!
	nullptr, // no particular mixer
};
