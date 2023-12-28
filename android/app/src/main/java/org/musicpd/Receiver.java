// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

package org.musicpd;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.util.Log;

import java.util.Set;

public class Receiver extends BroadcastReceiver {

	private static final Set<String> BOOT_ACTIONS = Set.of(
			"android.intent.action.BOOT_COMPLETED",
			"android.intent.action.QUICKBOOT_POWERON"
	);

	@Override
	public void onReceive(Context context, Intent intent) {
		Log.d("Receiver", "onReceive: " + intent);
		if (BOOT_ACTIONS.contains(intent.getAction())) {
			if (Preferences.getBoolean(context,
							    Preferences.KEY_RUN_ON_BOOT,
							    false)) {
				final boolean wakelock =
					Preferences.getBoolean(context,
									Preferences.KEY_WAKELOCK, false);
				Main.start(context, wakelock);
			}
		}
	}
}
