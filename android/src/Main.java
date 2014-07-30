/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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

package org.musicpd;

import android.app.Activity;
import android.os.Bundle;
import android.os.Build;
import android.os.Handler;
import android.os.Message;
import android.widget.TextView;
import android.util.Log;

public class Main extends Activity implements Runnable {
	private static final String TAG = "MPD";

	Thread thread;

	TextView textView;

	final Handler quitHandler = new Handler() {
			public void handleMessage(Message msg) {
				textView.setText("Music Player Daemon has quit");

				// TODO: what now?  restart?
			}
		};

	@Override protected void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);

		if (!Loader.loaded) {
			TextView tv = new TextView(this);
			tv.setText("Failed to load the native MPD libary.\n" +
				   "Report this problem to us, and include the following information:\n" +
				   "ABI=" + Build.CPU_ABI + "\n" +
				   "PRODUCT=" + Build.PRODUCT + "\n" +
				   "FINGERPRINT=" + Build.FINGERPRINT + "\n" +
				   "error=" + Loader.error);
			setContentView(tv);
			return;
		}

		if (thread == null || !thread.isAlive()) {
			thread = new Thread(this, "NativeMain");
			thread.start();
		}

		textView = new TextView(this);
		textView.setText("Music Player Daemon is running"
				 + "\nCAUTION: this version is EXPERIMENTAL!");
		setContentView(textView);
	}

	@Override public void run() {
		Bridge.run(this);
		quitHandler.sendMessage(quitHandler.obtainMessage());
	}
}
