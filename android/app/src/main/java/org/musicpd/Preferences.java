package org.musicpd;

import static android.content.Context.MODE_PRIVATE;

import android.content.Context;
import android.content.SharedPreferences;

public class Preferences {
    private static final String TAG = "Settings";

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
        final SharedPreferences.Editor editor = prefs.edit();
        editor.putBoolean(key, value);
        editor.apply();
    }

    public static boolean getBoolean(Context context, String key, boolean defValue) {
        final SharedPreferences prefs = get(context);

        return prefs != null ? prefs.getBoolean(key, defValue) : defValue;
    }
}