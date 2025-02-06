// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project
package org.musicpd.utils

import android.content.ActivityNotFoundException
import android.content.Context
import android.content.Intent
import android.net.Uri
import android.provider.Settings
import android.util.Log

private const val TAG = "IntentUtils"

fun openAppSettings(
    context: Context,
    packageName: String
) {
    try {
        context.startActivity(Intent().apply {
            setAction(Settings.ACTION_APPLICATION_DETAILS_SETTINGS)
            setData(Uri.parse("package:$packageName"))
            addCategory(Intent.CATEGORY_DEFAULT)
            addFlags(Intent.FLAG_ACTIVITY_NO_HISTORY)
            addFlags(Intent.FLAG_ACTIVITY_EXCLUDE_FROM_RECENTS)
        })
    } catch (e: ActivityNotFoundException) {
        Log.e(
            TAG,
            "failed to open app settings for package: $packageName", e
        )
    }
}
