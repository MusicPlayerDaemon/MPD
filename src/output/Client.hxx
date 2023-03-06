// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_OUTPUT_CLIENT_HXX
#define MPD_OUTPUT_CLIENT_HXX

/**
 * An interface between the #AudioOutput and the #Player.
 */
class AudioOutputClient {
public:
	/**
	 * Notify the client that we have consumed a few chunks.  This
	 * is called from within the output thread.  The client may
	 * perform actions to refill the #MusicPipe.
	 */
	virtual void ChunksConsumed() = 0;

	/**
	 * The #AudioOutput has modified the "enabled" flag, and the
	 * client shall make the #AudioOutput apply this new setting.
	 * This is called from any thread, one which can't send an
	 * AudioOutput::Command to the output thread; only the client
	 * can do that safely.
	 */
	virtual void ApplyEnabled() = 0;
};

#endif
