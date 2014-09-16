/*
 * Copyright 2003-2018 The Music Player Daemon Project
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
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.util.Log;
import android.widget.CompoundButton;
import android.widget.CompoundButton.OnCheckedChangeListener;
import android.widget.LinearLayout;
import android.widget.TextView;
import android.widget.ToggleButton;

public class Settings extends Activity {
	private static final String TAG = "Settings";
	private Main.Client mClient;
	private TextView mTextView;
	private ToggleButton mButton;
	private LinearLayout mLayout;

	private static final int MSG_ERROR = 0;
	private static final int MSG_STOPPED = 1;
	private static final int MSG_STARTED = 2;

	private Handler mHandler = new Handler(new Handler.Callback() {

		@Override
		public boolean handleMessage(Message msg) {
			switch (msg.what) {
			case MSG_ERROR:
				Log.d(TAG, "onError");
				final String error = (String) msg.obj;
				mTextView.setText("Failed to load the native MPD libary.\n" +
						  "Report this problem to us, and include the following information:\n" +
						  "SUPPORTED_ABIS=" + String.join(", ", Build.SUPPORTED_ABIS) + "\n" +
						  "PRODUCT=" + Build.PRODUCT + "\n" +
						  "FINGERPRINT=" + Build.FINGERPRINT + "\n" +
						  "error=" + error);
				mButton.setChecked(false);
				mButton.setEnabled(false);
				break;
			case MSG_STOPPED:
				Log.d(TAG, "onStopped");
				if (mButton.isEnabled()) // don't overwrite previous error message
					mTextView.setText("Music Player Daemon is not running");
				mButton.setEnabled(true);
				mButton.setChecked(false);
				break;
			case MSG_STARTED:
				Log.d(TAG, "onStarted");
				mTextView.setText("Music Player Daemon is running"
								  + "\nCAUTION: this version is EXPERIMENTAL!");
				mButton.setChecked(true);
				break;
			}
			return true;
		}
	});

	private OnCheckedChangeListener mOnCheckedChangeListener = new OnCheckedChangeListener() {

		@Override
		public void onCheckedChanged(CompoundButton buttonView, boolean isChecked) {
			if (mClient != null) {
				if (isChecked)
					mClient.start();
				else
					mClient.stop();
			}
		}
	};

	@Override
	protected void onCreate(Bundle savedInstanceState) {
		mTextView = new TextView(this);
		mTextView.setText("");

		mButton = new ToggleButton(this);
		mButton.setOnCheckedChangeListener(mOnCheckedChangeListener);

		mLayout = new LinearLayout(this);
		mLayout.setOrientation(LinearLayout.VERTICAL);
		mLayout.addView(mButton);
		mLayout.addView(mTextView);

		setContentView(mLayout);

		super.onCreate(savedInstanceState);
	}

	@Override
	protected void onStart() {
		mClient = new Main.Client(this, new Main.Client.Callback() {
			@Override
			public void onStopped() {
				mHandler.removeCallbacksAndMessages(null);
				mHandler.sendEmptyMessage(MSG_STOPPED);
			}

			@Override
			public void onStarted() {
				mHandler.removeCallbacksAndMessages(null);
				mHandler.sendEmptyMessage(MSG_STARTED);
			}

			@Override
			public void onError(String error) {
				mHandler.removeCallbacksAndMessages(null);
				mHandler.sendMessage(Message.obtain(mHandler, MSG_ERROR, error));
			}

			@Override
			public void onLog(int priority, String msg) {
			}
		});
		super.onStart();
	}

	@Override
	protected void onStop() {
		mClient.release();
		mClient = null;
		super.onStop();
	}
}
