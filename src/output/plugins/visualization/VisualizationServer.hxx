// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef VISUALIZATION_SERVER_HXX_INCLUDED
#define VISUALIZATION_SERVER_HXX_INCLUDED 1

#include "VisualizationClient.hxx"

#include "SoundAnalysis.hxx"

#include "config/Net.hxx"
#include "event/CoarseTimerEvent.hxx"
#include "event/ServerSocket.hxx"
#include "net/SocketAddress.hxx"
#include "net/UniqueSocketDescriptor.hxx"

struct AudioFormat;
struct ConfigBlock;

namespace Visualization {

class SoundInfoCache;

/**
 * \class VisualizationServer
 *
 * \brief A socker server handling visualization clients
 *
 * \sa \ref vis_out_arch "Architecture"
 *
 *
 * This class handles binding one or more sockets & accepting incoming
 * connections. For each such incoming connection, it will allocate a
 * VisualizationClient instance to represent that client.
 *
 * The clients require both a PCM data cache and a SoundAnalysis instance to do
 * their work. The former must be shared with the plugin that ultimately owns
 * this class as well as the VisualizationClient instances, while the latter is
 * cheaply copyable and so each client simply gets its own copy.
 *
 * The problem is that both must know the audio format in use (i.e. the number
 * of samples per second and the number of channels), and that is only known
 * when the plugin is "opened". Therefore this class can be represented by, yes,
 * a finite state machine:
 *
 \code

   Open --- OnPluginOpened() ---> HavePcmData
	 ^				  |
	 |				  |
	 +---- OnPluginClosed() ----------+

 \endcode
 *
 * When a new client connection is opened:
 *
 * - if we are in state Open, we cannot provide the client with sound analysis
 *	 information nor a reference to the PCM cache
 * - if we are in state HavePcmData, we can share a reference to our PCM cache
 *	 along with the salient information needed for sound analysis
 *
 * On state change:
 *
 * - from Open to HavePcmData, we can update all extant clients with a
 *	 shared reference to the PCM cache as well as the new sound analysis
 *	 information
 * - from HavePcmData to Open, we need to tell all extant clients to
 *	 drop their PCM cache references, as well as their sound analysis
 *	 information
 *
 *
 */

class VisualizationServer : public ServerSocket {

	/// only valid when the plugin is open
	struct HavePcmData {
		// I wish C++ had a `not_null` class
		std::shared_ptr<SoundInfoCache> pcache;
	};
	/// Present state-- v means closed, v means opened (the plugin, that is)
	std::variant<std::monostate, HavePcmData> state;
	/// maximum number of clients permitted; zero => unlimited
	size_t max_clients;

	/* Clients have both a reference to the PCM cache as well as a
	 * SoundAnalysis instance while the plugin is opened. We'll create new
	 * clients with our present state.
	 * Nb. that VisualizationClient, being a BufferedSocket, is not
	 * copy constructable, and so must be emplaced. */
	std::list<VisualizationClient> clients;
	/// invoked periodically to clean-up dead clients
	CoarseTimerEvent reaper;
	// Audio analysis parameters
	SoundAnalysisParameters sound_params;

public:
	VisualizationServer(EventLoop &event_loop, const char *bind_to_address,
			    uint16_t port, size_t max_clients,
			    const SoundAnalysisParameters &params);

	void ReapClients() noexcept;
	void OnPluginOpened(const std::shared_ptr<SoundInfoCache> &pcache);
	void OnPluginClosed();

protected:
	/* Invoked by `ServerSocket`, on its event loop, when a new client connects
	 *
	 * \a fd is the file descriptor of our new socket, \a address is the
	 * remote address, and \a uid is the effective UID of the client if \a
	 * fd is a UNIX-domain socket */
	virtual void OnAccept(UniqueSocketDescriptor fd, SocketAddress address,
			      int uid) noexcept override;

};

} // namespace Visualization

#endif // VISUALIZATION_SERVER_HXX_INCLUDED
