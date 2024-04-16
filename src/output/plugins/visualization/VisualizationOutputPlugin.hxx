// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_VISUALIZATION_OUTPUT_PLUGIN_HXX
#define MPD_VISUALIZATION_OUTPUT_PLUGIN_HXX

/**
 * \page vis_out The Visualization Output Plugin
 *
 * \section vis_out_intro Introduction
 *
 * Unlike most output plugins, which provide sound data in one format or
 * another, this plugin provides data \e derived from the current audio stream
 * convenient for authors of
 * <a href="https://en.wikipedia.org/wiki/Music_visualization">music visualizers</a>.
 *
 * \section vis_out_back Background
 *
 * This plugin started from a conversation on the #mpd IRC channel. I asked
 * about the best way to implement a music visualizer as a remote MPD
 * client. All of the MPD visualizers of which I was aware used the
 * <a href="https://mpd.readthedocs.io/en/latest/plugins.html#fifo">fifo</a>
 * output plugin and consequently had to be run on the same host as the MPD
 * daemon. It was suggested that I write an output plugin that would stream the
 * data needed to implement a visualizer.
 *
 * I submitted an
 * <a href="https://github.com/MusicPlayerDaemon/MPD/pull/1449">RFC</a>
 * in which we kicked around the ideas of implementing the simplest protocol
 * first, and of exposing sound information not only over a network protocol
 * (like, say, the HttpdOutput plugin), but also over the MPD
 * <a href="https://mpd.readthedocs.io/en/latest/protocol.html">client protocol</a>.
 *
 * This plugin is the result of those conversations.
 *
 * \subsection vis_out_prior Prior Art
 *
 * Music visualization sources which I consulted before settling on this approach:
 *
 * - This <a href="https://github.com/MusicPlayerDaemon/MPD/pull/488">PR</a>
 *   proposed solving the problem by implementing an output plugin that would
 *   stream the raw PCM data over TCP, the idea being that the remote visualizer
 *   would do the sound analysis client-side. The PR was discarded as being
 *   redundant with the \c HttpdOutput plugin. I would also observe that such a
 *   solution sends \e far more data on the wire than is needed for
 *   visualization.
 *
 * - <a href="https://github.com/ncmpcpp/ncmpcpp">ncmpcpp</a> uses the
 *   FifoOutput plugin, and as such can only provide the visualization feature
 *   when it's being run locally. The sound analysis is limited, as well (on
 *   which more below).
 *
 * - <a href="https://github.com/dpayne/cli-visualizer">cli-visualizer</a> will
 *   work with the MPD FIFO output plugin (again assuming the MPD daemon is
 *   running locally). Limited sound analysis, as well.
 *
 * - <a href="http://www.geisswerks.com/about_milkdrop.html">MilkDrop</a>:
 *   reading the source code was very instructive in terms of sound analysis for
 *   music visualization; that aspect of this plugin is largely based on it.
 *
 *
 * \section vis_out_plugin The Plugin
 *
 * A new output plugin "visualization" is provided. The plugin "plays" PCM data
 * by caching it. It provides continuous sound analysis at some caller-supplied
 * offset of the current song time consisting of PCM samples, Fourier
 * coefficients, frequency information & so forth. Like \c HttpdOutput and
 * \c SnapcastOutput, the plugin includes a socket server that will provide a
 * network endpoint at which clients can access sound analysis. In the future,
 * analysis may be made available over the MPD client protocol as well.
 *
 *
 * \subsection vis_output_plugin_arch Architecture
 *
 * VisualizationOutput is like HttpdOutput and SnapcastOutput in that it
 * implements both an AudioOutput and a socket server. Unlike those two
 * implementations, I chose not to multiply inherit from AudioOutput &
 * ServerSocket. The are more details \ref vis_out_arch "here", but briefly: I
 * chose to have VisualizationOutput \e own a ServerSocket rather than \e be a
 * ServerSocket, and pushed the responsibility for caching PCM data down into
 * class SoundInfoCache on which both my output plugin & socket server
 * depend. This arrangement is intended to both break-up circular dependencies
 * among the classes involved as well as reduce the number of places in which
 * objects are accessed by multiple threads.
 *
 *
 * \subsection vis_output_plugin_analysis Sound Analysis
 *
 * Given audio data in raw PCM format, a number of steps may be taken to analyze
 * that data & produce information useful to visualizer authors. This section
 * describes the full pipeline briefly. Most of these steps are optional at
 * request-time and are described in greater detail in the relevant docs.
 *
 * - the PCM data may optionally be damped by taking a weighted average between
 *   the current values & prior values in the time domain; this will have the
 *   effect of reducing noise in the higher frequency ranges
 *
 * - the PCM data may have a window function applied to it in the time domain
 *   around the time of interest; such a function has the effect of "dialing
 *   down" audio samples further from the timestamp of interest and again will
 *   reduce higher-frequency noise; the size of the window may be configured to
 *   incorporate more or less data as desired.
 *
 * - the resulting PCM data will be shifted into the frequency domain by
 *   application of the Discrete Fourier Transform
 *
 * - the human ear can only distinguish frequencies from (about) 200Hz to
 *   20000Hz, and in practice musical sound information doesn't show much
 *   activity above 10000Hz; it is therefore convenient to throw out frequency
 *   data outside some (configurable) frequency range
 *
 * - it is also convenient to divide the resulting spectrum into a few coarse
 *   bands, such as bass/mids/trebs. This is computationally non-trivial because
 *   perceptually, frequency is not linear, it's logrithmic. A change of one
 *   octave corresponds to a doubling in frequency. Intuitively, this means that
 *   the difference betwenn 200 & 300Hz is much greater than the difference
 *   betwen 5000 & 5100Hz, e.g. The plugin will peform this service for clients.
 *
 * - it can also be useful to maintain a weighted time average of the activity
 *   in each frequency range for purposes of beat detection
 *
 *
 * \subsection vis_output_protocol The Protocol
 *
 * The specifics of sound analysis are defined in the plugin configuration & are
 * identical for all clients.  When clients connect, they provide the frame rate
 * at which they would like to receive updates and the offset between
 * client-side render time & server-side song time (to account for network lag,
 * client-side buffering & the time needed to render each frame). Once that
 * initial handshake is complete, the server will stream updates containing
 * sound analysis results at regular intervals to the client.
 *
 * Note that each update need only be based on relatively few samples (Winamp,
 * e.g. used 576). This will keep the data transferred on the wire small (at
 * least by comparison to, say, the httpd output plugin which of course needs to
 * send the entire song).  Casting the protocol in terms of client-side FPS
 * allows us to avoid a "request/response" protocol & simply stream until the
 * client goes away.
 *
 * The protocol specification has its own \ref vis_out_protocol "page".
 *
 *
 */

extern const struct AudioOutputPlugin visualization_output_plugin;

#endif
