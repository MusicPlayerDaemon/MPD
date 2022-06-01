// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "VisualizationOutputPlugin.hxx"
#include "SoundAnalysis.hxx"
#include "SoundInfoCache.hxx"
#include "VisualizationServer.hxx"

#include "Log.hxx"
#include "config/Block.hxx"
#include "event/Call.hxx"
#include "lib/fmt/ThreadIdFormatter.hxx"
#include "output/Interface.hxx"
#include "output/OutputPlugin.hxx"
#include "util/Domain.hxx"

#include <chrono>

namespace Visualization {

/**
 * \page vis_out_protocol Visualization Network Protocol
 *
 * See \ref vis_out "RFC: Visualization Output Plugin" for background.
 *
 *
 * \section vis_out_protocol_timing Timing
 *
 * In order to deliver sound data to the client at the proper time, the protocol
 * needs to take into account:
 *
 * - network latency: the delta between writing the sound data to the socket &
 *   its receipt at the client
 *
 * - player buffering: the player may buffer sound data (mplayer, for instance,
 *   buffers half a second's worth of audio before beginning playback by
 *   default)
 *
 * - render time: the client presumably wishes the current frame to appear
 *   on-screen at the moment the current sound information is ending
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
 * to the client socket. If we denote that time offset (i.e. the render time +
 * one-way latency minus the buffer time) by tau, then the server should wait
 * max(0, -tau) ms to write the first frame.
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
 * designed his own after reviewing the Shoutcast & Ultravox
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
 *     corrupted data, client-side disruptions & so forth
 *   - all messages conform to the "type-length-value" (TLV) format
 *     beloved of parser writers
 *
 * Responses to the intial
 * <a href="https://github.com/MusicPlayerDaemon/MPD/pull/1449">RFC</a> also
 * informed the protocol's first implementation: I've stripped out all but the
 * essentials in pursuit of a minimally effective protocol that is still
 * extensible should it prove useful
 *
 *
 * \subsection vis_out_protocol_proto_overview Overview
 *
 * The protocol is a combination of request/response as well as streaming. After
 * an initial handshake (client goes first) the server will begin streaming
 * messages to the client; i.e. at the interval the client specified during the
 * initial handshake the server will send FRAME messages containing sound
 * information useful for visualizers. The client need not request these
 * messages or does the client need to acknowledge them in any way.
 *
 * Schematically, a conversation looks like this:
 *
 \verbatim
   Client                                                   Server

   desired protocol version
   tau (buffer offset)
   frame rate               --------- CLIHLO --------->
     ...

                            <-------- SRVHLO --------- offered protocol version

                            <-------- FRAME ---------  samples, spectrum
                                                       | 1/fps sec
                            <-------- FRAME ---------  samples, spectrum
                                       ...
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

   +---------+-----------------------+-----------------+-----------------------+--------+
   |(prefix) | TYPE (16-bit unsigned)|     LENGTH      |    PAYLOAD            |  CHECK |
   |         | class | message type  | 16-bits unsigned| LENGTH bytes          | 1 byte |
   |---------|-------+---------------|-----------------|-----------------------+--------+
   |63ac84003| 4 bits|   12 bits     | (max len 65535) | format is msg-specfic |    00  |
   +---------+-----------------------+-----------------+-----------------------+--------+

 \endverbatim
 *
 * Notes:
 *
 * - the prefix is only prepended to FRAME messages to enable clients to "lock
 *   on" to a pre-existing stream of data; 0x63ac4003 were the first four bytes
 *   I pulled from \c /dev/urandom on my dev workstation on Monday, September 04.
 *
 * - the message type is comprised of two values packed into a u16_t:
 *
 *   - class: (type & 0xf000) >> 12:
 *     - 00: handshake
 *     - 01: streaming (FRAME, e.g.)
 *
 *   - message type: (type & 0ffff) see below for values
 *
 * - the "length" field is the length of the \e payload \e only (i.e. \e not the
 *   length of the entire message)
 *
 * - the "check" byte is intended as a sanity test & shall always be zero
 *   Although, what would the client do if the check failed? There's no
 *   provision in this protocol to re-request the frame. Discard it, I suppose.
 *
 * The following subsections define the PAYLOAD portion of the above messages.
 *
 * \subsubsection vis_out_protocol_proto_clihlo CLIHLO
 *
 * No prefix. The class is 0x0 (handshake) & the message type is 0x000.
 *
 * Payload:
 *
 \verbatim

  +---------------+---------------+---------------+---------------+
  | major version | minor version | requested FPS | requested TAU |
  | ------------- | ------------- |-------------- |---------------+
  |    uint8_t    |    uint8_t    |   uint16_t    |  int16_t      |
  +---------------+---------------+---------------+---------------+

 \endverbatim
 *
 * Payload size: 6 octets
 *
 * \subsubsection vis_out_protocol_proto_srvhlo SRVHLO
 *
 * No prefix. The class is 0x0 (handshake) & the message type is 0x001.
 *
 * Payload:
 *
 \verbatim

  +---------------+---------------+
  | major version | minor version |
  | ------------- | ------------- |
  |    uint8_t    |    uint8_t    |
  +---------------+---------------+

 \endverbatim
 *
 * \subsubsection vis_out_protocol_proto_frame FRAME
 *
 * Prefix. The class is 0x1 (streaming) & the message type is 0x000.
 *
 * Below, \c float denotes a floating-point value, expressed in IEEE 754
 * single-precision format, in big-endian byte order. \c complex denotes a pair
 * of floating-point values (the real & imaginary components of a complex
 * number, in that order) in the same format.
 *
 * Payload:
 *
 \code

  +----------+----------+-------------+-----------+----------+---------+---------+----------+------------+---------------+-----------------+
  | num_samp | num_chan | sample_rate | waveforms | num_freq | freq_lo | freq_hi | freq_off |   coeffs   | power_spectra | bass/mids/trebs |
  | -------- | -------- | ----------- | --------- | -------- | ------- | ------- | -------- | ---------- | ------------- | --------------- |
  | uint16_t |  uint8_t |  uint16_t   | see below | uint16_t |  float  |  float  | uint16_t | see below  |  see below    |   see below     |
  +----------+----------+-------------+-----------+----------+---------+---------+----------+------------+---------------+-----------------+

  waveforms:

  +----------------------+----------------------+-----+---------------------------------+
  | waveform for chan. 0 | waveform for chan. 1 | ... | waveform for chan. num_chan - 1 |
  | -------------------- | -------------------- | ... | ------------------------------- |
  | float |  ... | float | float |  ... | float | ... | float |  ...            | float |
  | -------------------- | -------------------- | ... | ------------------------------- |
  |  (num_samp floats)   |   (num_samp floats)  | ... |   (num_samp floats)             |
  +----------------------+----------------------+-----+---------------------------------+

      total: num_samp * num_chan * 4 octets

  coeffs:

  +--------------------------+--------------------------+-----+-------------------------------------+
  | freq. domain for chan. 0 | freq. domain for chan 1. | ... | freq. domain for chan. num_chan - 1 |
  | ------------------------ + -------------------------+---- + ----------------------------------- |
  | complex |  ... | complex | complex |  ... | complex | ... | complex | complex |  ...  | complex |
  | ------------------------ +--------------------------+-----+-------------------------------------|
  |     num_freq complex     |  num_freq complex        | ... | num_freq complex                    |
  +--------------------------+--------------------------+-----+-------------------------------------+

      total: num_chan * num_freq * 8 octets

  power spectra:

  +-----------------------------+-----+---------------------------------------+
  | power spectrum for chan. 0  | ... | power spectrum for chan. num_chan - 1 |
  | --------------------------- +-----+ ------------------------------------- |
  | float | float | ... | float | ... | float | float |    ...        | float |
  | --------------------------- + --- + ------------------------------------- |
  |       num_freq floats       | ... |     num_freq floats                   |
  +-----------------------------+-----+---------------------------------------+

      total: num_chan * num_freq * 4 octets

  bass/mids/trebs

  +-----------------------------+-----+----------------------------------------+
  | bass/mids/trebs for chan. 0 | ... | bass/mids/trebs for chan. num_chan - 1 |
  | --------------------------- +-----+ -------------------------------------- |
  |   float  |  float  |  float | ... |   float  |   float  |   float          |
  +-----------------------------+-----+----------------------------------------+

      total: num_chan * 12 octets

  payload size: 17 + num_samp * num_chan * 4  + num_chan * num_freq * 8  + num_chan * num_freq * 4 + num_chan * 12
             = 17 + 4 * num_chan * (num_samp + 3 * num_freq + 3)

 \endcode
 *
 * - \c num_samp: the number of audio samples used in this analysis: this is set
 *   in plugin confiugration and in practice needn't be particularly large (512
 *   is the default setting). This determines the number of values in
 *   \c waveforms, and in part the number of values in \c frequencies and
 *   \c power_spectra (see below)
 *
 * - \c num_chan: the number of audio channels used in this analysis: this is
 *   determined by the audio stream being played at any given time, but 2
 *   (i.e. stereo) is typical
 *
 * - \c sample_rate: the number of samples per second at which this audio stream
 *   is encoded (44100 is typical)
 *
 * - \c waveforms: the PCM data on which this analysis was based; there will be
 *   \c num_chan sets of num_samp floats (one for each channel, arranged one
 *   after the other; i.e. not interleaved)
 *
 * - \c num_freq: the number of frequency values returned for each waveform in
 *   this frame; this is a function the sample rate, the number of audio
 *   samples, and the frequency cutoffs with which the plugin was configured (on
 *   which more below)
 *
 * - \c freq_lo, \c freq_hi: the frequency range returned; this is set in plugin
 *   configuration.  The range of human perception is roughly 200Hz to 20,000Hz,
 *   but in practice musical sound data contains little information above 10-12K
 *   Hz, so a typical setting for this range is 200Hz and 10000Hz.
 *
 * - \c freq_off: the index corresponding to \c freq_lo; this can be used by the
 *   caller to map a Fourier coefficient to a frequency (see \c coeffs, below)
 *
 * - \c coeffs: the Fourier coefficients for each waveform, expressed as complex
 *   numbers; the i-th value in this range is the \c freq_off + \c i -th Fourier
 *   coefficient, corresponding to a frequency of
 *
     \code

       (freq_off +  i) * samp_rate
       ---------------------------   Hz
                num_samp

     \endcode
 *
 * The reason for this convention is that the plugin will _only_ return the
 * Fourier coefficients within the ranage defined by \c freq_lo & \c freq_hi.
 *
 * Note that Discrete Fourier Transforms of real-valued series (such as our PCM
 * waveform) display the Hermitian property:
 *
 \code
                  *
     C(i) = C(n-i)

 \endcode
 *
 * where '*' denotes complex conjugation. Many libraries take advantage of this
 * to save space by only returning the first n/2 + 1 Fourier coefficients (since
 * the remaining coefficients can be readily computed from those). The
 * application of a frequency window spoils this nice symmetry.
 *
 * - \c power_spectra: the power spectrum for each channel; this is merely the
 *   magnitude of the Fourier coefficent at each frequency. Strictly speaking
 *   the client could compute this for themselves, but this is such a frequently
 *   used value the plugin computes & transmits it as a convenience to the
 *   caller, There are again \c num_freq values.
 *
 * - bass/mids/trebs: once the frequency domain is truncated to the given
 *   bounds, the number of octaves therein is divided into three equal
 *   bands and the power in each band is summed (this is done separately
 *   for each channel)
 *
 * A number of these quantities won't change; they're defined in plugin
 * configuration; \c num_samp, \c freq_lo & \c freq_hi could, in principle, be
 * moved to the SRVHLO message.
 *
 * Furthermore, \c num_chan, \c sample_rate and hence \c num_freq are set at the
 * start of each new audio stream, and so could be communicated once at that
 * point & omitted from subsequent frames.
 *
 * That said, this would complicate client implementations for the sake of
 * saving a few bytes on the wire; I've chosen to simply communicate this
 * information in each frame.
 *
 *
 */

/**
 * \page vis_out_arch Layout of the Visualization Output Plugin
 *
 * \section vis_out_arch_intro Introduction
 *
 * There are, at the time of this writing, two other output plugins that provide
 * socket servers: HttpdOutput & SnapcastOutput. They both follow a similar
 * pattern in which the plugin subclasses both AudioOutput \e and
 * ServerSocket. Since I have chosen a different approach, I should both
 * describe the layout of VisualizationOutput and explain my choice.
 *
 * \section vis_out_arch_cyclic Cyclic Dependencies
 *
 * While they subclass privately (implying an "implemented-in-terms-of" rather
 * than "is-a" relationship with their superclasses), HttpdOutput &
 * SnapcastOutput in practice handle the duties of being both an AudioOutput and
 * a ServerSocket. This introduces not one but two cyclic dependencies in their
 * implementations:
 *
 * 1. the ServerSocket half of them is responsible for creating new clients, but
 * the clients are the ones who detect that their socket has been closed; they
 * then need a back-reference to signal their parent that they should be
 * destroyed (by calling RemoveClient() through their back-reference).
 *
 * 2. the AudioOutput half of them is responsible for pushing new data derived
 * from PCM data out to all their clients, while their clients request
 * information & service from their parent, again requiring a back reference
 * (GetCodecName() on the Snapcast client, e.g.)
 *
 * Cyclic dependencies carry with them drawbacks:
 *
 * - they increase compilation times because when one file in the cycle is
 *   changed, all the other translation units need to be recompiled
 *
 * - they increase coupling, increasing the chances that a change in
 *   one place will break others
 *
 * - code reuse becomes more difficult-- trying to hoist one file out involves
 *   bringing all the other files in the cycle along with it
 *
 * - unit testing becomes harder-- the smallest unit of testable
 *   funcationality becomes the union all the the translation units in the
 *   cycle
 *
 * \section vis_out_arch_threads Too Many Threads!
 *
 * This arrangement entails another problem: HttpdOutput & SnapcastOutput
 * instances have their methods invoked on two threads; the main I/O thread as
 * well as the player control thread. This means that access to some state needs
 * to be guarded by a mutex (in the case of HttpdOutput, the client list & the
 * pages), but \e not others (again in the case of HttpdOutput, content or
 * genre).
 *
 * \section vis_out_arch_demotion Breaking Dependency Cyles Through Demotion
 *
 * I instead chose to have VisualizationOutput \e be an AudioOutput, and \e own
 * a ServerSocket. The state & behavior required by both is pushed down into
 * class SoundInfoCache on which both depend. This arrangement breaks things up
 * in a few ways.
 *
 * Cycle 1 is broken up by having a one-way relationship only between the socket
 * server & clients. When a client detects that its socket has been closed, it
 * marks itself "dead" and will eventually be reaped by the server.
 *
 * Cycle 2 is broken by Lakos' method of "demotion": the functionality required
 * by both the output plugin & the various clients is pushed down into a
 * separate class SoundInfoCache. It is owned by the plugin, and referenced by
 * clients. When the plugin is disabled, the plugin is responsible for
 * cleaning-up the server, which will in turn clean-up all the clients, and only
 * then destroying the SoundInfoCache instance.
 *
 * In ASCII art:
 *
 \verbatim
    sound       +---------------------+               +---------------------+
 -- data ---->  | VisualizationOutput | --- owns ---> | VisualizationServer |
                +---------------------+               +---------------------+
                | Play()              |               | OnAccept()          |
                +---------------------+               +---------------------+
                         1 |                                     | 1
                           |                         +---owns----+
                           |                         |
                           |                         v *
                           |               +---------------------+
                          owns             | VisualizationClient |
                           |               +---------------------+
                           |                         | *
                           |    +----references------+
                           |    |
                         1 v    v 1
                    +----------------+
                    | SoundInfoCache |
                    +----------------+
 \endverbatim
 *
 * This arrangement also addresses the threading issue: other than creation &
 * destruction, the socket server has all of its methods invoked on the I/O
 * thread, and those of the plugin on the player control thread. The state that
 * needs to be guarded against access from multiple threads is localized in
 * SoundInfoCache.
 *
 *
 * \section vis_out_arch_promotion A Discarded Approach
 *
 * The \ref vis_out_back "idea" of having sound analysis accessible through the
 * MPD client
 * <a href="https://mpd.readthedocs.io/en/latest/protocol.html">protocol</a>
 * to me begged the question: why not have SoundInfoCache be owned directly by
 * MultipleOutputs? MPD clients could make requests directly via
 *
 \code
   partition.outputs.sound_info_cache.analyze(...);
 \endcode
 *
 * We could hand a reference to it to the visualization output plugin, and have
 * the plugin be solely responsible for serving the network protocol.
 *
 * I saw a few advantages to this:
 *
 * 1. Convenient access for the implementations of MPD client protocol commands
 *
 * 2. Users could get sound analysis via the MPD client protocol without having
 * to configure & enable an output plugin
 *
 * 3. General simplification-- the output plugin would only be responsible
 * for serving the network protocol
 *
 * All that said, I discarded this approach. If I wanted the sound analysis to
 * receive sound data post-cross-fade, post-replay gain and after any other
 * filtering, it was going to need to own an AudioOutputSource instance. Thing
 * is, when I open an AudioOutputSource I need:
 *
 * - the AudioFormat
 * - a reference to the MusicPipe
 * - the ReplayGain filter(s)
 * - any other filters
 *
 * MultipleOutputs doesn't know these; it's just got a bunch of
 * configuration. The configuration gets turned into these objects in
 * FilteredAudioOutput::Setup() and it's non-trivial to do so. The plumbing is
 * complex enough that I'm inclined to leave it where it is. So now we're at a
 * point where SoundInfoCache would need to own both an AudioOutputSource \e and
 * a FilteredAudioOutput... at which point it starts to look very much like an
 * AudioOutputControl (in other words, just another audio output under
 * MultipleOutputs).
 *
 *
 */

/**
 * \class VisualizationOutput
 *
 * \brief An output plugin that serves data useful for music visualizers
 *
 * \sa \ref vis_out_plugin_arch "Architecture"
 *
 *
 * Both the fifo & pipe output plugins can be used to directly access the PCM
 * audio data, and so can (and have been) used to implement music visualizers
 * for MPD. They are, however, limited to clients running on the same host as
 * MPD. This output plugin will stream PCM samples along with derived
 * information useful for visualizers (the Fourier transform, bass/mids/trebs,
 * and so forth) over one or more network connections, to allow true MPD client
 * visualizers.
 *
 *
 */

class VisualizationOutput: public AudioOutput {

	/* When the plugin is enabled, we actually "open" the server (which is
	 * to say, bind the socket & begin accepting incoming connections) */
	VisualizationServer server;
	/* This will be null unless the plugin is open; it's a `shared_ptr`
	 * because we share references with the socket servers and the
	 * `VisualizationClient` instances representing active connections */
	std::shared_ptr<SoundInfoCache> pcache;
	/// The number of seconds' worth of audio data to be cached
	std::chrono::seconds cache_duration;

public:
	static AudioOutput* Create(EventLoop &event_loop,
				   const ConfigBlock &cfg_block) {
		return new VisualizationOutput(event_loop, cfg_block);
	}
	VisualizationOutput(EventLoop &event_loop,
			    const ConfigBlock &cfg_block);

	virtual ~VisualizationOutput() override; // We have virtuals, so...

public:

	////////////////////////////////////////////////////////////////////////
	//		     AudioOutput Interface			      //
	////////////////////////////////////////////////////////////////////////

	/**
	 * Enable the device.  This may allocate resources, preparing
	 * for the device to be opened.
	 *
	 * Throws on error.
	 */
	virtual void Enable() override;

	/**
	 * Disables the device. It is closed before this method is called.
	 */
	virtual void Disable() noexcept override;

	/**
	 * Really open the device-- mandatory.
	 *
	 * Throws on error.
	 *
	 * @param audio_format the audio format in which data is going
	 * to be delivered; may be modified by the plugin
	 */
	virtual void Open(AudioFormat &audio_format) override;

	/**
	 * Close the device-- mandatory.
	 */
	virtual void Close() noexcept override;

	/**
	 * Play a chunk of audio data-- mandatory. The method blocks until at
	 * least one audio frame is consumed.
	 *
	 * Throws on error.
	 *
	 * May throw #AudioOutputInterrupted after Interrupt() has
	 * been called.
	 *
	 * @return the number of bytes played (must be a multiple of
	 * the frame size)
	 */
	virtual size_t Play(std::span<const std::byte> src) override;

};

} // namespace Visualization

using std::make_unique;

const Domain vis_output_domain("vis_output");

Visualization::VisualizationOutput::VisualizationOutput(
	EventLoop &event_loop,
	const ConfigBlock &config_block):
	AudioOutput(FLAG_ENABLE_DISABLE | FLAG_PAUSE),
	server(event_loop,
		   config_block.GetBlockValue("bind_to_address"),
		   config_block.GetBlockValue("port", 8001U),
		   config_block.GetPositiveValue("max_clients", 0),
		   Visualization::SoundAnalysisParameters(config_block)),
	cache_duration(config_block.GetPositiveValue("cache_duration", 1))
{ }

Visualization::VisualizationOutput::~VisualizationOutput()
{ }

void
Visualization::VisualizationOutput::Enable() {

	FmtInfo(vis_output_domain, "VisualizationOutput::Enable({})", std::this_thread::get_id());

	BlockingCall(server.GetEventLoop(), [this](){
		server.Open();
	});

}

void
Visualization::VisualizationOutput::Disable() noexcept {

	FmtInfo(vis_output_domain, "VisualizationOutput::Disable({})", std::this_thread::get_id());

	BlockingCall(server.GetEventLoop(), [this](){
			server.Close();
		});

}

void
Visualization::VisualizationOutput::Open(AudioFormat &audio_format)
{
	FmtInfo(vis_output_domain, "VisualizationOutput::Open({})", std::this_thread::get_id());

	/* At this point, we know the audio format, so we can at this point
	 * instantiate the PCM data cache. */
	pcache = make_shared<Visualization::SoundInfoCache>(audio_format,
							    cache_duration);

	BlockingCall(server.GetEventLoop(), [this]() {
		server.OnPluginOpened(pcache);
	});
}

void
Visualization::VisualizationOutput::Close() noexcept
{
	FmtInfo(vis_output_domain, "VisualizationOutput::Close({})", std::this_thread::get_id());

	BlockingCall(server.GetEventLoop(), [this]() {
		server.OnPluginClosed();
	});

	pcache = nullptr;
}

size_t
Visualization::VisualizationOutput::Play(const std::span<const std::byte> src)
{
	pcache->Add(src.data(), src.size());
	return src.size();
}

const struct AudioOutputPlugin visualization_output_plugin = {
	"visualization",
	nullptr, // cannot serve as the default output
	&Visualization::VisualizationOutput::Create,
	nullptr, // no particular mixer
};
