// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project
package org.musicpd

import android.content.Context
import android.os.Build
import android.util.Log

object Loader {
    private const val TAG = "Loader"

    private var loaded: Boolean = false
    private var error: String? = null
    private val failReason: String get() = error ?: ""

    val isLoaded: Boolean get() = loaded

    init {
        load()
    }

    private fun load() {
        if (loaded) return
        loaded = try {
            error = null
            System.loadLibrary("mpd")
            Log.i(TAG, "mpd lib loaded")
            true
        } catch (e: Throwable) {
            error = e.message ?: e.javaClass.simpleName
            Log.e(TAG, "failed to load mpd lib: $failReason")
            false
        }
    }

    fun loadFailureMessage(context: Context): String {
        return context.getString(
            R.string.mpd_load_failure_message,
            Build.SUPPORTED_ABIS.joinToString(),
            Build.PRODUCT,
            Build.FINGERPRINT,
            failReason
        )
    }
}
