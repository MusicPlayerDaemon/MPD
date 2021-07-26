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

import java.util.LinkedList;

import android.Manifest;
import android.app.Activity;
import android.content.Context;
import android.content.SharedPreferences;
import android.content.SharedPreferences.Editor;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.util.Log;
import android.widget.ArrayAdapter;
import android.widget.CheckBox;
import android.widget.CompoundButton;
import android.widget.CompoundButton.OnCheckedChangeListener;
import android.widget.ListView;
import android.widget.TextView;
import android.widget.ToggleButton;

public class Settings extends Activity {
	private static final String TAG = "Settings";
	private Main.Client mClient;
	private TextView mTextStatus;
	private ToggleButton mRunButton;
	private boolean mFirstRun;
	private LinkedList<String> mLogListArray = new LinkedList<String>();
	private ListView mLogListView;
	private ArrayAdapter<String> mLogListAdapter;

	private static final int MAX_LOGS = 500;

	private static final int MSG_ERROR = 0;
	private static final int MSG_STOPPED = 1;
	private static final int MSG_STARTED = 2;
	private static final int MSG_LOG = 3;

	public static class Preferences {
		public static final String KEY_RUN_ON_BOOT ="run_on_boot";
		public static final String KEY_WAKELOCK ="wakelock";
		public static final String KEY_PAUSE_ON_HEADPHONES_DISCONNECT ="pause_on_headphones_disconnect";

		public static SharedPreferences get(Context context) {
			return context.getSharedPreferences(TAG, MODE_PRIVATE);
		}

		public static void putBoolean(Context context, String key, boolean value) {
			final SharedPreferences prefs = get(context);

			if (prefs == null)
				return;
			final Editor editor = prefs.edit();
			editor.putBoolean(key, value);
			editor.apply();
		}

		public static boolean getBoolean(Context context, String key, boolean defValue) {
			final SharedPreferences prefs = get(context);

			return prefs != null ? prefs.getBoolean(key, defValue) : defValue;
		}
	}

	private Handler mHandler = new Handler(new Handler.Callback() {
		@Override
		public boolean handleMessage(Message msg) {
			switch (msg.what) {
			case MSG_ERROR:
				Log.d(TAG, "onError");

				mClient.release();
				connectClient();

				mRunButton.setEnabled(false);
				mRunButton.setChecked(false);

				mTextStatus.setText((String)msg.obj);
				mFirstRun = true;
				break;
			case MSG_STOPPED:
				Log.d(TAG, "onStopped");
				mRunButton.setEnabled(true);
				if (!mFirstRun && Preferences.getBoolean(Settings.this, Preferences.KEY_RUN_ON_BOOT, false))
					mRunButton.setChecked(true);
				else
					mRunButton.setChecked(false);
				mFirstRun = true;
				mTextStatus.setText("");
				break;
			case MSG_STARTED:
				Log.d(TAG, "onStarted");
				mRunButton.setChecked(true);
				mFirstRun = true;
				mTextStatus.setText("MPD service started");
				break;
			case MSG_LOG:
				if (mLogListArray.size() > MAX_LOGS)
					mLogListArray.remove(0);
				String priority;
				switch (msg.arg1) {
				case Log.DEBUG:
					priority = "D";
					break;
				case Log.ERROR:
					priority = "E";
					break;
				case Log.INFO:
					priority = "I";
					break;
				case Log.VERBOSE:
					priority = "V";
					break;
				case Log.WARN:
					priority = "W";
					break;
				default:
					priority = "";
				}
				mLogListArray.add(priority + "/ " + (String)msg.obj);
				mLogListAdapter.notifyDataSetChanged();

				break;
			}
			return true;
		}
	});

	private final OnCheckedChangeListener mOnRunChangeListener = new OnCheckedChangeListener() {
		@Override
		public void onCheckedChanged(CompoundButton buttonView, boolean isChecked) {
			if (mClient != null) {
				if (isChecked) {
					mClient.start();
					if (Preferences.getBoolean(Settings.this,
							Preferences.KEY_WAKELOCK, false))
						mClient.setWakelockEnabled(true);
					if (Preferences.getBoolean(Settings.this,
							Preferences.KEY_PAUSE_ON_HEADPHONES_DISCONNECT, false))
						mClient.setPauseOnHeadphonesDisconnect(true);
				} else {
					mClient.stop();
				}
			}
		}
	};

	private final OnCheckedChangeListener mOnRunOnBootChangeListener = new OnCheckedChangeListener() {
		@Override
		public void onCheckedChanged(CompoundButton buttonView, boolean isChecked) {
			Preferences.putBoolean(Settings.this, Preferences.KEY_RUN_ON_BOOT, isChecked);
			if (isChecked && mClient != null && !mRunButton.isChecked())
				mRunButton.setChecked(true);
		}
	};

	private final OnCheckedChangeListener mOnWakelockChangeListener = new OnCheckedChangeListener() {
		@Override
		public void onCheckedChanged(CompoundButton buttonView, boolean isChecked) {
			Preferences.putBoolean(Settings.this, Preferences.KEY_WAKELOCK, isChecked);
			if (mClient != null && mClient.isRunning())
				mClient.setWakelockEnabled(isChecked);
		}
	};

	private final OnCheckedChangeListener mOnPauseOnHeadphonesDisconnectChangeListener = new OnCheckedChangeListener() {
		@Override
		public void onCheckedChanged(CompoundButton buttonView, boolean isChecked) {
			Preferences.putBoolean(Settings.this, Preferences.KEY_PAUSE_ON_HEADPHONES_DISCONNECT, isChecked);
			if (mClient != null && mClient.isRunning())
				mClient.setPauseOnHeadphonesDisconnect(isChecked);
		}
	};

	@Override
	protected void onCreate(Bundle savedInstanceState) {
		/* TODO: this sure is the wrong place to request
		   permissions - it will cause MPD to quit
		   immediately; we should request permissions when we
		   need them, but implementing that is complicated, so
		   for now, we do it here to give users a quick
		   solution for the problem */
		requestAllPermissions();

		setContentView(R.layout.settings);
		mRunButton = (ToggleButton) findViewById(R.id.run);
		mRunButton.setOnCheckedChangeListener(mOnRunChangeListener);

		mTextStatus = (TextView) findViewById(R.id.status);

		mLogListAdapter = new ArrayAdapter<String>(this, R.layout.log_item, mLogListArray);

		mLogListView = (ListView) findViewById(R.id.log_list);
		mLogListView.setAdapter(mLogListAdapter);
		mLogListView.setTranscriptMode(ListView.TRANSCRIPT_MODE_NORMAL);

		CheckBox checkbox = (CheckBox) findViewById(R.id.run_on_boot);
		checkbox.setOnCheckedChangeListener(mOnRunOnBootChangeListener);
		if (Preferences.getBoolean(this, Preferences.KEY_RUN_ON_BOOT, false))
			checkbox.setChecked(true);

		checkbox = (CheckBox) findViewById(R.id.wakelock);
		checkbox.setOnCheckedChangeListener(mOnWakelockChangeListener);
		if (Preferences.getBoolean(this, Preferences.KEY_WAKELOCK, false))
			checkbox.setChecked(true);

		checkbox = (CheckBox) findViewById(R.id.pause_on_headphones_disconnect);
		checkbox.setOnCheckedChangeListener(mOnPauseOnHeadphonesDisconnectChangeListener);
		if (Preferences.getBoolean(this, Preferences.KEY_PAUSE_ON_HEADPHONES_DISCONNECT, false))
			checkbox.setChecked(true);

		super.onCreate(savedInstanceState);
	}

	private void checkRequestPermission(String permission) {
		if (checkSelfPermission(permission) == PackageManager.PERMISSION_GRANTED)
			return;

		try {
			this.requestPermissions(new String[]{permission}, 0);
		} catch (Exception e) {
			Log.e(TAG, "requestPermissions(" + permission + ") failed",
			      e);
		}
	}

	private void requestAllPermissions() {
		if (android.os.Build.VERSION.SDK_INT < 23)
			/* we don't need to request permissions on
			   this old Android version */
			return;

		/* starting with Android 6.0, we need to explicitly
		   request all permissions before using them;
		   mentioning them in the manifest is not enough */

		checkRequestPermission(Manifest.permission.READ_EXTERNAL_STORAGE);
	}

	private void connectClient() {
		mClient = new Main.Client(this, new Main.Client.Callback() {

			private void removeMessages() {
				/* don't remove log messages */
				mHandler.removeMessages(MSG_STOPPED);
				mHandler.removeMessages(MSG_STARTED);
				mHandler.removeMessages(MSG_ERROR);
			}

			@Override
			public void onStopped() {
				removeMessages();
				mHandler.sendEmptyMessage(MSG_STOPPED);
			}

			@Override
			public void onStarted() {
				removeMessages();
				mHandler.sendEmptyMessage(MSG_STARTED);
			}

			@Override
			public void onError(String error) {
				removeMessages();
				mHandler.sendMessage(Message.obtain(mHandler, MSG_ERROR, error));
			}

			@Override
			public void onLog(int priority, String msg) {
				mHandler.sendMessage(Message.obtain(mHandler, MSG_LOG, priority, 0, msg));
			}
		});
	}

	@Override
	protected void onStart() {
		mFirstRun = false;
		connectClient();
		super.onStart();
	}

	@Override
	protected void onStop() {
		mClient.release();
		mClient = null;
		super.onStop();
	}
}
