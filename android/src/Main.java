/*
 * Copyright 2003-2021 The Music Player Daemon Project
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

import android.annotation.TargetApi;
import android.app.Notification;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.Service;
import android.content.BroadcastReceiver;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.ServiceConnection;
import android.media.AudioManager;
import android.os.Build;
import android.os.IBinder;
import android.os.PowerManager;
import android.os.RemoteCallbackList;
import android.os.RemoteException;
import android.util.Log;
import android.widget.RemoteViews;

import java.lang.reflect.Constructor;
import java.lang.reflect.Method;

public class Main extends Service implements Runnable {
	private static final String TAG = "Main";
	private static final String REMOTE_ERROR = "MPD process was killed";
	private static final int MAIN_STATUS_ERROR = -1;
	private static final int MAIN_STATUS_STOPPED = 0;
	private static final int MAIN_STATUS_STARTED = 1;

	private static final int MSG_SEND_STATUS = 0;
	private static final int MSG_SEND_LOG = 1;

	private Thread mThread = null;
	private int mStatus = MAIN_STATUS_STOPPED;
	private boolean mAbort = false;
	private String mError = null;
	private final RemoteCallbackList<IMainCallback> mCallbacks = new RemoteCallbackList<IMainCallback>();
	private final IBinder mBinder = new MainStub(this);
	private boolean mPauseOnHeadphonesDisconnect = false;
	private PowerManager.WakeLock mWakelock = null;

	static class MainStub extends IMain.Stub {
		private Main mService;
		MainStub(Main service) {
			mService = service;
		}
		public void start() {
			mService.start();
		}
		public void stop() {
			mService.stop();
		}
		public void setPauseOnHeadphonesDisconnect(boolean enabled) {
			mService.setPauseOnHeadphonesDisconnect(enabled);
		}
		public void setWakelockEnabled(boolean enabled) {
			mService.setWakelockEnabled(enabled);
		}
		public boolean isRunning() {
			return mService.isRunning();
		}
		public void registerCallback(IMainCallback cb) {
			mService.registerCallback(cb);
		}
		public void unregisterCallback(IMainCallback cb) {
			mService.unregisterCallback(cb);
		}
	}

	private synchronized void sendMessage(int what, int arg1, int arg2, Object obj) {
		int i = mCallbacks.beginBroadcast();
		while (i > 0) {
			i--;
			final IMainCallback cb = mCallbacks.getBroadcastItem(i);
			try {
				switch (what) {
				case MSG_SEND_STATUS:
					switch (arg1) {
					case MAIN_STATUS_ERROR:
						cb.onError((String)obj);
						break;
					case MAIN_STATUS_STOPPED:
						cb.onStopped();
						break;
					case MAIN_STATUS_STARTED:
						cb.onStarted();
						break;
					}
					break;
				case MSG_SEND_LOG:
					cb.onLog(arg1, (String) obj);
					break;
				}
			} catch (RemoteException e) {
			}
		}
		mCallbacks.finishBroadcast();
	}

	private Bridge.LogListener mLogListener = new Bridge.LogListener() {
		@Override
		public void onLog(int priority, String msg) {
			sendMessage(MSG_SEND_LOG, priority, 0, msg);
		}
	};

	@Override
	public IBinder onBind(Intent intent) {
		return mBinder;
	}

	@Override
	public int onStartCommand(Intent intent, int flags, int startId) {
		start();
		if (intent != null && intent.getBooleanExtra("wakelock", false))
			setWakelockEnabled(true);
		return START_STICKY;
	}

	@Override
	public void run() {
		if (!Loader.loaded) {
			final String error = "Failed to load the native MPD libary.\n" +
				"Report this problem to us, and include the following information:\n" +
				"SUPPORTED_ABIS=" + String.join(", ", Build.SUPPORTED_ABIS) + "\n" +
				"PRODUCT=" + Build.PRODUCT + "\n" +
				"FINGERPRINT=" + Build.FINGERPRINT + "\n" +
				"error=" + Loader.error;
			setStatus(MAIN_STATUS_ERROR, error);
			stopSelf();
			return;
		}
		synchronized (this) {
			if (mAbort)
				return;
			setStatus(MAIN_STATUS_STARTED, null);
		}
		Bridge.run(this, mLogListener);
		setStatus(MAIN_STATUS_STOPPED, null);
	}

	private synchronized void setStatus(int status, String error) {
		mStatus = status;
		mError = error;
		sendMessage(MSG_SEND_STATUS, mStatus, 0, mError);
	}

	private Notification.Builder createNotificationBuilderWithChannel() {
		final NotificationManager notificationManager = (NotificationManager) this.getSystemService(Context.NOTIFICATION_SERVICE);
		if (notificationManager == null)
			return null;

		final String id = "org.musicpd";
		final String name = "MPD service";
		final int importance = 3; /* NotificationManager.IMPORTANCE_DEFAULT */

		try {
			Class<?> ncClass = Class.forName("android.app.NotificationChannel");
			Constructor<?> ncCtor = ncClass.getConstructor(String.class, CharSequence.class, int.class);
			Object nc = ncCtor.newInstance(id, name, importance);

			Method nmCreateNotificationChannelMethod =
				NotificationManager.class.getMethod("createNotificationChannel", ncClass);
			nmCreateNotificationChannelMethod.invoke(notificationManager, nc);

			Constructor nbCtor = Notification.Builder.class.getConstructor(Context.class, String.class);
			return (Notification.Builder) nbCtor.newInstance(this, id);
		} catch (Exception e)
		{
			Log.e(TAG, "error creating the NotificationChannel", e);
			return null;
		}
	}

	private void start() {
		if (mThread != null)
			return;

		IntentFilter filter = new IntentFilter();
		filter.addAction(AudioManager.ACTION_AUDIO_BECOMING_NOISY);
		registerReceiver(new BroadcastReceiver() {
			@Override
			public void onReceive(Context context, Intent intent) {
				if (!mPauseOnHeadphonesDisconnect)
					return;
				if (intent.getAction() == AudioManager.ACTION_AUDIO_BECOMING_NOISY)
					pause();
			}
		}, filter);

		final Intent mainIntent = new Intent(this, Settings.class);
		mainIntent.setAction("android.intent.action.MAIN");
		mainIntent.addCategory("android.intent.category.LAUNCHER");
		final PendingIntent contentIntent = PendingIntent.getActivity(this, 0,
				mainIntent, PendingIntent.FLAG_CANCEL_CURRENT);

		Notification.Builder nBuilder;
		if (Build.VERSION.SDK_INT >= 26 /* Build.VERSION_CODES.O */)
		{
			nBuilder = createNotificationBuilderWithChannel();
			if (nBuilder == null)
				return;
		}
		else
			nBuilder = new Notification.Builder(this);

		Notification notification = nBuilder.setContentTitle(getText(R.string.notification_title_mpd_running))
			.setContentText(getText(R.string.notification_text_mpd_running))
			.setSmallIcon(R.drawable.notification_icon)
			.setContentIntent(contentIntent)
			.build();

		mThread = new Thread(this);
		mThread.start();

		startForeground(R.string.notification_title_mpd_running, notification);
		startService(new Intent(this, Main.class));
	}

	private void stop() {
		if (mThread != null) {
			if (mThread.isAlive()) {
				synchronized (this) {
					if (mStatus == MAIN_STATUS_STARTED)
						Bridge.shutdown();
					else
						mAbort = true;
				}
			}
			try {
				mThread.join();
				mThread = null;
				mAbort = false;
			} catch (InterruptedException ie) {}
		}
		setWakelockEnabled(false);
		stopForeground(true);
		stopSelf();
	}

	private void pause() {
		if (mThread != null) {
			if (mThread.isAlive()) {
				synchronized (this) {
					if (mStatus == MAIN_STATUS_STARTED)
						Bridge.pause();
				}
			}
		}
	}

	private void setPauseOnHeadphonesDisconnect(boolean enabled) {
		mPauseOnHeadphonesDisconnect = enabled;
	}

	private void setWakelockEnabled(boolean enabled) {
		if (enabled && mWakelock == null) {
			PowerManager pm = (PowerManager)getSystemService(Context.POWER_SERVICE);
			mWakelock = pm.newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, TAG);
			mWakelock.acquire();
			Log.d(TAG, "Wakelock acquired");
		} else if (!enabled && mWakelock != null) {
			mWakelock.release();
			mWakelock = null;
			Log.d(TAG, "Wakelock released");
		}
	}

	private boolean isRunning() {
		return mThread != null && mThread.isAlive();
	}

	private void registerCallback(IMainCallback cb) {
		if (cb != null) {
			mCallbacks.register(cb);
			sendMessage(MSG_SEND_STATUS, mStatus, 0, mError);
		}
	}

	private void unregisterCallback(IMainCallback cb) {
		if (cb != null) {
			mCallbacks.unregister(cb);
		}
	}

	/*
	 * Client that bind the Main Service in order to send commands and receive callback
	 */
	public static class Client {

		public interface Callback {
			public void onStarted();
			public void onStopped();
			public void onError(String error);
			public void onLog(int priority, String msg);
		}

		private boolean mBound = false;
		private final Context mContext;
		private Callback mCallback;
		private IMain mIMain = null;

		private final IMainCallback.Stub mICallback = new IMainCallback.Stub() {

			@Override
			public void onStopped() throws RemoteException {
				mCallback.onStopped();
			}

			@Override
			public void onStarted() throws RemoteException {
				mCallback.onStarted();
			}

			@Override
			public void onError(String error) throws RemoteException {
				mCallback.onError(error);
			}

			@Override
			public void onLog(int priority, String msg) throws RemoteException {
				mCallback.onLog(priority, msg);
			}
		};

		private final ServiceConnection mServiceConnection = new ServiceConnection() {

			@Override
			public void onServiceConnected(ComponentName name, IBinder service) {
				synchronized (this) {
					mIMain = IMain.Stub.asInterface(service);
					try {
						if (mCallback != null)
							mIMain.registerCallback(mICallback);
					} catch (RemoteException e) {
						if (mCallback != null)
							mCallback.onError(REMOTE_ERROR);
					}
				}
			}

			@Override
			public void onServiceDisconnected(ComponentName name) {
				if (mCallback != null)
					mCallback.onError(REMOTE_ERROR);
			}
		};

		public Client(Context context, Callback cb) throws IllegalArgumentException {
			if (context == null)
				throw new IllegalArgumentException("Context can't be null");
			mContext = context;
			mCallback = cb;
			mBound = mContext.bindService(new Intent(mContext, Main.class), mServiceConnection, Context.BIND_AUTO_CREATE);
		}

		public boolean start() {
			synchronized (this) {
				if (mIMain != null) {
					try {
						mIMain.start();
						return true;
					} catch (RemoteException e) {
					}
				}
				return false;
			}
		}

		public boolean stop() {
			synchronized (this) {
				if (mIMain != null) {
					try {
						mIMain.stop();
						return true;
					} catch (RemoteException e) {
					}
				}
				return false;
			}
		}

		public boolean setPauseOnHeadphonesDisconnect(boolean enabled) {
			synchronized (this) {
				if (mIMain != null) {
					try {
						mIMain.setPauseOnHeadphonesDisconnect(enabled);
						return true;
					} catch (RemoteException e) {
					}
				}
				return false;
			}
		}

		public boolean setWakelockEnabled(boolean enabled) {
			synchronized (this) {
				if (mIMain != null) {
					try {
						mIMain.setWakelockEnabled(enabled);
						return true;
					} catch (RemoteException e) {
					}
				}
				return false;
			}
		}

		public boolean isRunning() {
			synchronized (this) {
				if (mIMain != null) {
					try {
						return mIMain.isRunning();
					} catch (RemoteException e) {
					}
				}
				return false;
			}
		}

		public void release() {
			if (mBound) {
				synchronized (this) {
					if (mIMain != null && mICallback != null) {
						try {
							if (mCallback != null)
								mIMain.unregisterCallback(mICallback);
						} catch (RemoteException e) {
						}
					}
				}
				mBound = false;
				mContext.unbindService(mServiceConnection);
			}
		}
	}

	/*
	 * start Main service without any callback
	 */
	public static void start(Context context, boolean wakelock) {
		Intent intent = new Intent(context, Main.class)
			.putExtra("wakelock", wakelock);

		if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O)
			/* in Android 8+, we need to use this method
			   or else we'll get "IllegalStateException:
			   app is in background" */
			context.startForegroundService(intent);
		else
			context.startService(intent);
	}
}
