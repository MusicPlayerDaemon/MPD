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

	public static native void run(Context context, LogListener logListener);
	public static native void shutdown();
	public static native void pause();
	public static native void playPause();
	public static native void playNext();
	public static native void playPrevious();
}
