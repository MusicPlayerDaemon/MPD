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

#ifndef MPD_VISUALIZATION_OUTPUT_PLUGIN_HXX
#define MPD_VISUALIZATION_OUTPUT_PLUGIN_HXX

/**
 * \page vis_out RFC: Visualiation Output Plugin
 *
 * \section vis_out_intro Introduction
 *
 * This plugin started from a conversation on the #mpd IRC channel. I asked
 * about the best way to implement a music visualizer as a remote <a
 * href="https://musicpd.org">MPD</a> client. All the current MPD visualizers of
 * which I'm aware use the <a
 * href="https://mpd.readthedocs.io/en/latest/plugins.html#fifo">fifo</a> output
 * plugin and hence must be run on the same host as the MPD daemon.
 *
 * The response I got was a suggestion that I write an output plugin that would
 * \e just stream the data needed to implement a remote visualizer. I've begun
 * work on such a plugin, but before I spend too much time implementing it I
 * would like to lay out my proposal & solicit feedback.
 *
 * The codebase uses <a href="https://www.doxygen.nl>Doxygen</a>-style comments,
 * so I'm presenting this RFC as a few doxygen pages in the first files I'd be
 * adding to the project.
 *
 *
 * \section vis_out_prior Prior Art
 *
 * Music visualization sources which I consulted before settling on this
 * proposal:
 *
 * - This <a href="https://github.com/MusicPlayerDaemon/MPD/pull/488>PR</a>
 *   proposed solving this problem by implementing an output plugin that would
 *   stream the raw PCM data over TCP, the idea being that the remote visualizer
 *   would do the sound analysis client-side. The PR was discarded as being
 *   redundant with the <a
 *   href="https://mpd.readthedocs.io/en/latest/plugins.html#httpd">httpd</a>
 *   output plugin. I would also observe that such a solution sends far more
 *   data on the wire than is needed for visualization (on which more below).
 *
 * - <a href="https://github.com/ncmpcpp/ncmpcpp">ncmpcpp</a> uses the fifo
 *   output plugin, and as such can only provide the visualization feature when
 *   it's being run locally. The sound analysis is limited, as well (see below)
 *
 * - <a href="https://github.com/dpayne/cli-visualizer">cli-visualizer</a> will
 *   work with the MPD fifo (again assuming the MPD daemon is running
 *   locally). Limited sound analysis, as well.
 *
 * - <a href="http://www.geisswerks.com/about_milkdrop.html">MilkDrop</a>:
 *   reading the source code was very instructive in terms of sound analysis for
 *   music visualization; that aspect of this proposal is largely based on it.
 *
 *
 * \section vis_out_proposal The Proposal
 *
 * A new output plugin "visualization" will be implemented. The plugin will
 * cache recent PCM data. The plugin will also be a ServerSocket. When clients
 * connect, they will provide the details of the sound analysis they would like
 * performed, the frame rate at which they would like to receive updates and the
 * offset between client-side render time & server-side song time (to account
 * for network lag, client-side buffering & the time needed to render each
 * frame). Once that initial handshake is complete, the server will stream
 * updates containing sound analysis results at regular intervals to the
 * client.
 *
 * \subsection vis_output_proposal_analysis Sound Analysis
 *
 * Given audio data in raw PCM format, a number of steps may be taken to
 * analyze that data & produce infromation useful to visualizers:
 *
 * - the PCM data may optionally be damped by taking a weighted average between
 *   the current values & prior values in the time domain; this will have the
 *   effect of reducing noise in the higher frequency ranges
 *
 * - the PCM data may have a "window function" applied to it in the time domain
 *   around the time of interest; such a function has the effect of "dialing
 *   down" audio samples further from the timestamp of interest and again will
 *   reduce higher-frequency noise; the size of the window may be configured to
 *   incorporate more or less data as desired.
 *
 * - the resulting PCM data will be shifted into the frequency domain by
 *   application of the Discrete Fourier Transform
 *
 * - the human ear can only distinguish frequence from (about) 200Hz to 20000Hz,
 *   and in practice musical sound information doesn't show much activity above
 *   10000Hz; it is therefore convenient to throw out frequency data outside
 *   some (client-configurable) range
 *
 * - it is also convenient to divide the resulting spectrum into a few coarse
 *   bands, such as bass/mids/trebs. This is computationally non-trivial because
 *   perceptually, frequency is not linear, it's logrithmic. A change of one
 *   octave corresponds to a doubling in frequency. Intuitively, this means that
 *   the difference betwenn 200 & 300Hz is much greater than the difference
 *   betwen 5000 & 5100Hz, e.g. The plugin will peform this service for
 *   each client.
 *
 * - it can also be useful to maintain a weighted time average of the activity
 *   in each frequency range for purposes of beat detection
 *
 *
 * \subsection vis_output_protocol The Protocol
 *
 * Note that each update need only be based on relatively few samples (Winamp,
 * e.g. uses 576). This will keep the data transferred on the wire small (at
 * least by comparison to, say, the httpd output plugin which of course needs to
 * send the entire song).  Casting the protocol in terms of client-side FPS
 * allows us to avoid a "request/response" protocol & simply stream until the
 * client goes away.
 *
 * I've broken out the detailed protocol specification into its own
 * \ref vis_out_protocol "page".
 *
 *
 */

extern const struct AudioOutputPlugin visualization_output_plugin;

#endif
