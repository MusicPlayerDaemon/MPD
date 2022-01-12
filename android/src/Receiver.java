/*
 * Copyright (C) 2003-2021 The Music Player Daemon Project
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
