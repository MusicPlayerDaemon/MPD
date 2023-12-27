package org.musicpd.ui

import android.content.Context
import android.util.Log
import androidx.lifecycle.ViewModel
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import org.musicpd.Main
import org.musicpd.Preferences

private const val MAX_LOGS = 500

class SettingsViewModel : ViewModel() {

    private var mClient: Main.Client? = null

    private val _logItemFLow = MutableStateFlow(listOf<String>())
    val logItemFLow: StateFlow<List<String>> = _logItemFLow

    data class StatusUiState(
        val statusMessage: String = "",
        val running: Boolean = false
    )

    private val _statusUIState = MutableStateFlow(StatusUiState())
    val statusUIState: StateFlow<StatusUiState> = _statusUIState.asStateFlow()

    fun addLogItem(priority: Int, message: String) {
        if (_logItemFLow.value.size > MAX_LOGS) {
            _logItemFLow.value = _logItemFLow.value.drop(1)
        }

        val priorityString: String = when (priority) {
            Log.DEBUG -> "D"
            Log.ERROR -> "E"
            Log.INFO -> "I"
            Log.VERBOSE -> "V"
            Log.WARN -> "W"
            else -> ""
        }

        _logItemFLow.value = _logItemFLow.value + ("$priorityString/$message")
    }

    fun updateStatus(message: String, running: Boolean) {
        _statusUIState.value = StatusUiState(message, running)
    }

    fun setClient(client: Main.Client) {
        mClient = client
    }

    fun removeClient() {
        mClient?.release()
        mClient = null
    }

    fun startMPD(context: Context) {
        mClient?.start()
        if (Preferences.getBoolean(
                context,
                Preferences.KEY_WAKELOCK, false
            )
        ) mClient?.setWakelockEnabled(true)
        if (Preferences.getBoolean(
                context,
                Preferences.KEY_PAUSE_ON_HEADPHONES_DISCONNECT, false
            )
        ) mClient?.setPauseOnHeadphonesDisconnect(true)
    }

    fun stopMPD() {
        mClient?.stop()
    }

    fun setWakelockEnabled(enabled: Boolean) {
        mClient?.setWakelockEnabled(enabled)
    }

    fun setPauseOnHeadphonesDisconnect(enabled: Boolean) {
        mClient?.setPauseOnHeadphonesDisconnect(enabled)
    }
}