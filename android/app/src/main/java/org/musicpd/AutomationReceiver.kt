package org.musicpd

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent

class AutomationReceiver : BroadcastReceiver() {
    override fun onReceive(context: Context, intent: Intent) {

        when(intent.action) {
            "org.musicpd.action.StartService" -> {
                val wakelock = Preferences.getBoolean(
                    context,
                    Preferences.KEY_WAKELOCK, false
                )
                Main.startService(context, wakelock)
            }
            "org.musicpd.action.StopService" -> {
                context.startService(Intent(context, Main::class.java)
                    .setAction(Main.SHUTDOWN_ACTION))
            }
        }
    }
}
