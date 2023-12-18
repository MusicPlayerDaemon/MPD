// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

package org.musicpd;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.util.Log;

public class Receiver extends BroadcastReceiver {
	@Override
	public void onReceive(Context context, Intent intent) {
		Log.d("Receiver", "onReceive: " + intent);
		if (intent.getAction() == "android.intent.action.BOOT_COMPLETED") {
			if (Settings.Preferences.getBoolean(context,
							    Settings.Preferences.KEY_RUN_ON_BOOT,
							    false)) {
				final boolean wakelock =
					Settings.Preferences.getBoolean(context,
									Settings.Preferences.KEY_WAKELOCK, false);
				Main.start(context, wakelock);
			}
		}
	}
}
