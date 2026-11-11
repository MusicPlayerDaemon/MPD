// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project
package org.musicpd

import android.content.Context
import android.media.AudioAttributes
import android.media.AudioFocusRequest
import android.media.AudioManager
import android.os.Build
import android.util.Log

/**
 * Requests the audio focus on behalf of MPD's audio output and pauses
 * playback when another app takes it away.
 *
 * Without the audio focus, some audio routes (e.g. Android Auto) do
 * not play MPD's sound at all.
 */
class AudioFocusHelper(context: Context) : Bridge.AudioFocus,
    AudioManager.OnAudioFocusChangeListener {

    companion object {
        private const val TAG = "AudioFocus"
    }

    private val audioManager =
        context.getSystemService(Context.AUDIO_SERVICE) as AudioManager

    /**
     * The request object for Android 8+; null on older versions.
     */
    private val focusRequest: AudioFocusRequest? =
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O)
            AudioFocusRequest.Builder(AudioManager.AUDIOFOCUS_GAIN)
                .setAudioAttributes(
                    AudioAttributes.Builder()
                        .setUsage(AudioAttributes.USAGE_MEDIA)
                        .setContentType(AudioAttributes.CONTENT_TYPE_MUSIC)
                        .build()
                )
                .setOnAudioFocusChangeListener(this)
                .build()
        else
            null

    override fun requestAudioFocus() {
        val result =
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O)
                audioManager.requestAudioFocus(focusRequest!!)
            else
                @Suppress("DEPRECATION")
                audioManager.requestAudioFocus(
                    this, AudioManager.STREAM_MUSIC,
                    AudioManager.AUDIOFOCUS_GAIN
                )

        if (result != AudioManager.AUDIOFOCUS_REQUEST_GRANTED) {
            /* e.g. during a phone call */
            Log.w(TAG, "Audio focus request denied, pausing")
            Bridge.pause()
        }
    }

    override fun abandonAudioFocus() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O)
            audioManager.abandonAudioFocusRequest(focusRequest!!)
        else
            @Suppress("DEPRECATION")
            audioManager.abandonAudioFocus(this)
    }

    override fun onAudioFocusChange(focusChange: Int) {
        when (focusChange) {
            AudioManager.AUDIOFOCUS_LOSS,
            AudioManager.AUDIOFOCUS_LOSS_TRANSIENT -> {
                Log.i(TAG, "Audio focus lost, pausing")
                Bridge.pause()
            }

            /* AUDIOFOCUS_LOSS_TRANSIENT_CAN_DUCK is ignored;
               since Android 8, the system lowers our volume
               automatically */
        }
    }
}
