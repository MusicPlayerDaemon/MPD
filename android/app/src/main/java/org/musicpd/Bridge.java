// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

package org.musicpd;

import android.content.Context;

/**
 * Bridge to native code.
 */
public class Bridge {

	/* used by jni */
	public interface LogListener {
		public void onLog(int priority, String msg);
	}

	/* used by jni */
	public interface AudioFocus {
		/**
		 * Called by MPD's audio output when it begins playing
		 * (or resumes after a pause).  This may be called
		 * from any thread.
		 */
		public void requestAudioFocus();

		/**
		 * Called by MPD's audio output when it pauses or
		 * closes.  This may be called from any thread.
		 */
		public void abandonAudioFocus();
	}

	public static native void run(Context context, LogListener logListener,
				      AudioFocus audioFocus);
	public static native void shutdown();
	public static native void pause();
	public static native void playPause();
	public static native void playNext();
	public static native void playPrevious();
}
