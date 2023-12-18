// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

package org.musicpd;

import android.util.Log;

public class Loader {
	private static final String TAG = "MPD";

	public static boolean loaded = false;
	public static String error;

	static {
		try {
			System.loadLibrary("mpd");
			loaded = true;
		} catch (UnsatisfiedLinkError e) {
			Log.e(TAG, e.getMessage());
			error = e.getMessage();
		}
	}
}
