// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project
package org.musicpd

import android.app.Notification
import android.app.NotificationManager
import android.app.PendingIntent
import android.app.Service
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.media.AudioManager
import android.os.Build
import android.os.IBinder
import android.os.Looper
import android.os.PowerManager
import android.os.PowerManager.WakeLock
import android.os.RemoteCallbackList
import android.os.RemoteException
import android.util.Log
import androidx.annotation.OptIn
import androidx.core.app.ServiceCompat
import androidx.media3.common.util.UnstableApi
import androidx.media3.session.MediaSession
import dagger.hilt.android.AndroidEntryPoint
import org.musicpd.Bridge.LogListener
import org.musicpd.data.LoggingRepository
import java.lang.reflect.Constructor
import javax.inject.Inject

@AndroidEntryPoint
class Main : Service(), Runnable {
    companion object {
        private const val TAG = "Main"
        private const val WAKELOCK_TAG = "mpd:wakelockmain"

        private const val MAIN_STATUS_ERROR = -1
        private const val MAIN_STATUS_STOPPED = 0
        private const val MAIN_STATUS_STARTED = 1

        private const val MSG_SEND_STATUS = 0

        const val SHUTDOWN_ACTION: String = "org.musicpd.action.ShutdownMPD"

        /*
	 * start Main service without any callback
	 */
        @JvmStatic
        fun startService(context: Context, wakelock: Boolean) {
            val intent = Intent(context, Main::class.java)
                .putExtra("wakelock", wakelock)

            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) /* in Android 8+, we need to use this method
			   or else we'll get "IllegalStateException:
			   app is in background" */
                context.startForegroundService(intent)
            else context.startService(intent)
        }
    }

    private lateinit var mpdApp: MPDApplication
    private lateinit var mpdLoader: Loader

    private var mThread: Thread? = null
    private var mStatus = MAIN_STATUS_STOPPED
    private var mAbort = false
    private var mError: String? = null
    private val mCallbacks = RemoteCallbackList<IMainCallback>()
    private val mBinder: IBinder = MainStub(this)
    private var mPauseOnHeadphonesDisconnect = false
    private var mWakelock: WakeLock? = null

    private var mMediaSession: MediaSession? = null

    @JvmField
    @Inject
    var logging: LoggingRepository? = null

    internal class MainStub(private val mService: Main) : IMain.Stub() {
        override fun start() {
            mService.start()
        }

        override fun stop() {
            mService.stop()
        }

        override fun setPauseOnHeadphonesDisconnect(enabled: Boolean) {
            mService.setPauseOnHeadphonesDisconnect(enabled)
        }

        override fun setWakelockEnabled(enabled: Boolean) {
            mService.setWakelockEnabled(enabled)
        }

        override fun isRunning(): Boolean {
            return mService.isRunning
        }

        override fun registerCallback(cb: IMainCallback) {
            mService.registerCallback(cb)
        }

        override fun unregisterCallback(cb: IMainCallback) {
            mService.unregisterCallback(cb)
        }
    }

    override fun onCreate() {
        super.onCreate()
        mpdLoader = Loader
    }

    @Synchronized
    private fun sendMessage(
        @Suppress("SameParameterValue") what: Int,
        arg1: Int,
        arg2: Int,
        obj: Any?
    ) {
        var i = mCallbacks.beginBroadcast()
        while (i > 0) {
            i--
            val cb = mCallbacks.getBroadcastItem(i)
            try {
                when (what) {
                    MSG_SEND_STATUS -> when (arg1) {
                        MAIN_STATUS_ERROR -> cb.onError(obj as String?)
                        MAIN_STATUS_STOPPED -> cb.onStopped()
                        MAIN_STATUS_STARTED -> cb.onStarted()
                    }
                }
            } catch (ignored: RemoteException) {
            }
        }
        mCallbacks.finishBroadcast()
    }

    private val mLogListener = LogListener { priority, msg ->
        logging?.addLogItem(priority, msg)
    }

    override fun onBind(intent: Intent): IBinder {
        return mBinder
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        if (intent?.action == SHUTDOWN_ACTION) {
            stop()
        } else {
            start()
            if (intent?.getBooleanExtra(
                    "wakelock",
                    false
                ) == true
            ) setWakelockEnabled(true)
        }
        return START_REDELIVER_INTENT
    }

    override fun run() {
        synchronized(this) {
            if (mAbort) return
            setStatus(MAIN_STATUS_STARTED, null)
        }
        Bridge.run(this, mLogListener)
        setStatus(MAIN_STATUS_STOPPED, null)
    }

    @Synchronized
    private fun setStatus(status: Int, error: String?) {
        mStatus = status
        mError = error
        sendMessage(MSG_SEND_STATUS, mStatus, 0, mError)
    }

    private fun createNotificationBuilderWithChannel(): Notification.Builder? {
        val notificationManager = getSystemService(NOTIFICATION_SERVICE) as? NotificationManager
            ?: return null

        val id = "org.musicpd"
        val name = "MPD service"
        val importance = 3 /* NotificationManager.IMPORTANCE_DEFAULT */

        try {
            val ncClass = Class.forName("android.app.NotificationChannel")
            val ncCtor = ncClass.getConstructor(
                String::class.java,
                CharSequence::class.java,
                Int::class.javaPrimitiveType
            )
            val nc = ncCtor.newInstance(id, name, importance)

            val nmCreateNotificationChannelMethod =
                NotificationManager::class.java.getMethod("createNotificationChannel", ncClass)
            nmCreateNotificationChannelMethod.invoke(notificationManager, nc)

            val nbCtor: Constructor<*> = Notification.Builder::class.java.getConstructor(
                Context::class.java, String::class.java
            )
            return nbCtor.newInstance(this, id) as Notification.Builder
        } catch (e: Exception) {
            Log.e(TAG, "error creating the NotificationChannel", e)
            return null
        }
    }

    @OptIn(markerClass = [UnstableApi::class])
    private fun start() {
        if (mThread != null) return

        val filter = IntentFilter()
        filter.addAction(AudioManager.ACTION_AUDIO_BECOMING_NOISY)
        registerReceiver(object : BroadcastReceiver() {
            override fun onReceive(context: Context, intent: Intent) {
                if (!mPauseOnHeadphonesDisconnect) return
                if (intent.action === AudioManager.ACTION_AUDIO_BECOMING_NOISY) pause()
            }
        }, filter)

        val mainIntent = Intent(this, MainActivity::class.java)
        mainIntent.setAction("android.intent.action.MAIN")
        mainIntent.addCategory("android.intent.category.LAUNCHER")
        val contentIntent = PendingIntent.getActivity(
            this, 0,
            mainIntent, PendingIntent.FLAG_CANCEL_CURRENT or PendingIntent.FLAG_IMMUTABLE
        )

        val nBuilder: Notification.Builder?
        if (Build.VERSION.SDK_INT >= 26 /* Build.VERSION_CODES.O */) {
            nBuilder = createNotificationBuilderWithChannel()
            if (nBuilder == null) return
        } else nBuilder = Notification.Builder(this)

        val notification =
            nBuilder.setContentTitle(getText(R.string.notification_title_mpd_running))
                .setContentText(getText(R.string.notification_text_mpd_running))
                .setSmallIcon(R.drawable.notification_icon)
                .setContentIntent(contentIntent)
                .build()

        if (mpdLoader.isLoaded) {
            mThread = Thread(this).apply { start() }
        }

        val player = MPDPlayer(Looper.getMainLooper())
        mMediaSession = MediaSession.Builder(this, player).build()

        startForeground(R.string.notification_title_mpd_running, notification)
        startService(Intent(this, Main::class.java))
    }

    private fun stop() {
        mMediaSession?.let {
            it.release()
            mMediaSession = null
        }
        mThread?.let { thread ->
            if (thread.isAlive) {
                synchronized(this) {
                    if (mStatus == MAIN_STATUS_STARTED) Bridge.shutdown()
                    else mAbort = true
                }
            }
            try {
                thread.join()
                mThread = null
                mAbort = false
            } catch (ie: InterruptedException) {
                Log.e(TAG, "failed to join", ie)
            }
        }
        setWakelockEnabled(false)
        ServiceCompat.stopForeground(this, ServiceCompat.STOP_FOREGROUND_REMOVE)
        stopSelf()
    }

    private fun pause() {
        if (mThread?.isAlive == true) {
            synchronized(this) {
                if (mStatus == MAIN_STATUS_STARTED) Bridge.pause()
            }
        }
    }

    private fun setPauseOnHeadphonesDisconnect(enabled: Boolean) {
        mPauseOnHeadphonesDisconnect = enabled
    }

    private fun setWakelockEnabled(enabled: Boolean) {
        if (enabled) {
            val wakeLock =
                mWakelock ?: run {
                    val pm = getSystemService(POWER_SERVICE) as PowerManager
                    pm.newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, WAKELOCK_TAG).also {
                        mWakelock = it
                    }
                }
            wakeLock.acquire(10 * 60 * 1000L /*10 minutes*/)
            Log.d(TAG, "Wakelock acquired")
        } else {
            mWakelock?.let {
                it.release()
                mWakelock = null
            }
            Log.d(TAG, "Wakelock released")
        }
    }

    private val isRunning: Boolean
        get() = mThread?.isAlive == true

    private fun registerCallback(cb: IMainCallback?) {
        if (cb != null) {
            mCallbacks.register(cb)
            sendMessage(MSG_SEND_STATUS, mStatus, 0, mError)
        }
    }

    private fun unregisterCallback(cb: IMainCallback?) {
        if (cb != null) {
            mCallbacks.unregister(cb)
        }
    }
}
