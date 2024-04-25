// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef VISUALIZATION_CLIENT_HXX_INCLUDED
#define VISUALIZATION_CLIENT_HXX_INCLUDED 1

#include "SoundAnalysis.hxx"
#include "Protocol.hxx"

#include "event/BufferedSocket.hxx"
#include "event/FineTimerEvent.hxx"
#include "net/SocketError.hxx"
#include "net/UniqueSocketDescriptor.hxx"

#include <cstddef>
#include <optional>
#include <variant>
#include <vector>

namespace Visualization {

class SoundInfoCache;

/**
 * \class VisualizationClient
 *
 * \brief Represents a TCP connection to one visualization client
 *
 *
 * This class implements the server side of the MPD visualization protocol,
 * version 1, for a single client.
 *
 * The \ref vis_out_plugin_protocol_proto "protocol" suggests a finite state
 * machine (FSM):
 *
 * events:
 *
 * - read ready
 * - write complete
 * - timer fired
 * - plugin opened
 * - plugin closed
 *
 * actions
 *
 * - schedule write
 * - schedule read
 * - cancel write
 * - cancel read
 * - schedule timer(interval)
 *
 \code
                          +------+
                    +---->| Init | (read scheduled)
        read ready, |     +------+
        need more   |      |   |
                    +------+   | read ready, recv CLIHLO,
                               | schedule a write
                               |
                               v
                          +--------+
                     +--> | SRVHLO |-----------------------------+
      write complete,|    +--------+                             |
      more to write  |     | |    |                              | write complete,
                     +-----+ |    |                              | plugin closed
                             |    +----+                         | (cancel write)
                             |         | write complete,         |
                        +----+         | plugin open,            |
 write complete,        |              | tau < 0                 |
   plugin open,         |              | (cancel write)          |
   tau >= 0             |              | (schedule timer(-tau))  |
 (update analysis)      |              |     or                  |
 (schedule write)       |              | failed analysis         v
 (schedule timer(freq)) |              | (cancel write)         +--------+
                        |              | (schedule timer(freq)) | Closed |
                        v              v                        +--------+
             +------------+          +---------+
       +---->| FrameReady |          | Waiting |<----+
       |     +------------+          +---------+     |
       |      |    ^   |               ^  |  |       |
       +------+    |   |               |  |  |       |
 write complete,   |   +---------------+  |  +-------+
 more to write     |    write complete    |       no sound analysis
                   |    (cancel write)    |    (schedule timer(freq))
                   |                      |
                   +----------------------+
                        timer fired
                   (schedule timer(freq))
                      (schedule write)

 \endcode
 *
 * This is complicated by the fact that the output plugin that owns us may, at
 * any given point in time, be "open" or "closed"; it is only when open that we
 * know the format of the PCM data being played, and hence that this client may
 * have a reference to the PCM data cache along with a `SoundAnalysis` instance
 * necessary for performing sound analsysis.
 *
 * 1. instances start life waiting for the CLIHLO message (state :=> Init)
 *
 * 2. on read ready (state must be Init):
 *
 *    1) complete the read
 *
 *    2) compose the SRVHLO message
 *
 *    3) schedule a write
 *
 *    4) state :=> SrvHlo
 *
 * 3. on write ready (state must be SrvHlo)
 *
 *    1) write the current frame
 *
 *    2) branch:
 *
 *       - if the plugin is closed, state :=> Closed
 *       - if the client needs the first frame & the plugin is open
 *         + compose the frame
 *         + schedule a write
 *         + state :=> FrameReady
 *       - else
 *         + schedule the timer for -tau ms
 *         + state :=> Waiting
 *
 * 3. on write ready (state must be FrameReady)
 *
 *    1) write the current frame
 *
 *    2) schedule the timer for 1/fps ms
 *
 *    3) state :=> Waiting
 *
 * 4. on timer firing (state must be Waiting)
 *
 *    1) if the plugin is open:
 *
 *       - compose the next frame
 *       - schedule a write
 *       - state :=> FrameReady
 *
 *
 */

class VisualizationClient : BufferedSocket {

	Visualization::SoundAnalysisParameters sound_params;
	size_t num_samp;

	/// Data available to us when the visualization output plugin is open
	struct HavePcmData {
		// I wish C++ had a `not_null` class
		std::shared_ptr<Visualization::SoundInfoCache> pcache;
		Visualization::SoundAnalysis analysis;
	};
	/// Plugin open/closed state-- cf. PluginIsOpen()
	std::variant<std::monostate, HavePcmData> pcm_state;

	/// The protocol can be represented as an FSM
	enum class ProtocolState {
		/* FSM initial state; the socket has been established, but no
		 * communication has taken place; we are expecting a CLIHLO
		 * message to arrive (i.e. a READ/POLLIN notification) */
		Init,
		/* CLIHLO has arrived, we've composed the SRVHLO and are waiting
		 * for the socket to become available for write */
		SrvHlo,
		/* The handshake has been completed, but the plugin is currently
		 * closed, so we can't perform sound analysis */
		ProtocolClosed,
		/// Handshake complete, waiting for the timer to fire
		Waiting,
		/* Handshake complete, frame composed, waiting for the socket to
		 * become available for write */
		FrameReady,
		/// The socket has been closed and this instance may be reaped
		Done,
	} protocol_state;

	/// Information available to us once we've parsed the CLIHLO message
	struct HaveClientInfo {
		std::chrono::milliseconds tau;
		std::chrono::milliseconds freq; // 1/fps
	};
	/* A tuple whose first member is the offset from song time at which this
	 * client has requested sound analysis, and the second is the interval
	 * at which frames shall be sent (1/fps)-- both are only available to us
	 * after the CLIHLO message has been parsed and we are in state SrvHlo
	 * or later. */
	std::optional<HaveClientInfo> timings;
	/// Timer governing frame transmission
	FineTimerEvent timer;
	/* Next frame to be transmitted (if any) in serialized format
	 * (i.e. ready to be written directly); empty `vector` denotes no such
	 * frame */
	std::vector<std::byte> next_frame;

public:
	/* Constructor invoked when a new client connects & the plugin is
	   closed */
	VisualizationClient(
		UniqueSocketDescriptor fd, EventLoop &event_loop,
		const Visualization::SoundAnalysisParameters &params);
	/// Constructor invoked when a new client connects & the plugin is open
	VisualizationClient(
		UniqueSocketDescriptor fd, EventLoop &event_loop,
		const Visualization::SoundAnalysisParameters &params,
		const std::shared_ptr<Visualization::SoundInfoCache> &pcache);
	virtual ~VisualizationClient();

	/// Invoked by the server when the plugin is opened
	void OnPluginOpened(
		const std::shared_ptr<Visualization::SoundInfoCache> &pcache);
	/// Invoked by the server when the plugin is closed
	void OnPluginClosed();
	bool
	IsClosed() const noexcept {
		return ProtocolState::Done == protocol_state;
	}

protected:

	/////////////////////////////////////////////////////////////////////////
	//		BufferedSocket interface			       //
	/////////////////////////////////////////////////////////////////////////

	virtual BufferedSocket::InputResult
	OnSocketInput(std::span<std::byte> src) noexcept override;
	virtual void OnSocketError(std::exception_ptr ep) noexcept override;
	virtual void OnSocketClosed() noexcept override;

	/**
	 * Invoked when an event has occurred on this socket. \a flags
	 * will be a bitmask made of members of the EPollEvents enumeration.
	 * For reference:
	 *
	 * - READ = EPOLLIN = 1
	 * - WRITE = EPOLLOUT = 4
	 * - ERROR = EPOLLERR = 8
	 * - HANGUP = EPOLLHUP = 16
	 *
	 */
	virtual void OnSocketReady(unsigned flags) noexcept override;

private:

	/// Update our sound analysis
	bool ComposeSoundAnalysisFrame();
	/* Handle the first frame-- if tau < 0 schedule the timer for -tau ms,
	 * else write a frame immediately */
	void HandleFirstFrame();
	/// Handle a socket event while in state FrameReady
	void HandleFrameReady(unsigned flags);
	/// Handle a socket event while in state SrvHlo
	void HandleSrvHlo(unsigned flags);
	/// Utility function-- log a socket_error_t after an attempted write
	void LogSocketWriteError(const socket_error_t &err) const noexcept;
	/* Timer callback-- invoked when it's time to compose the next sound
	 * analysis frame */
	void OnTimer() noexcept;
	bool
	PluginIsOpen() const {
		return 0 != pcm_state.index();
	}
	/* Close our underlying socket, drop our shared cache & shift state to
	 * Done */
	void Shutdown() noexcept;
	bool WriteFrame();

};

} // namespace Visualization

#endif // VISUALIZATION_CLIENT_HXX_INCLUDED
