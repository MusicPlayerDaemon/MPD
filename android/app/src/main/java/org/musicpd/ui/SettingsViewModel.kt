package org.musicpd.ui

import android.content.Context
import androidx.lifecycle.ViewModel
import dagger.hilt.android.lifecycle.HiltViewModel
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import org.musicpd.MainServiceClient
import org.musicpd.Preferences
import org.musicpd.data.LoggingRepository
import javax.inject.Inject


@HiltViewModel
class SettingsViewModel @Inject constructor(
    private var loggingRepository: LoggingRepository
) : ViewModel() {
    private var mClient: MainServiceClient? = null

    data class StatusUiState(
        val statusMessage: String = "",
        val running: Boolean = false
    )

    private val _statusUIState = MutableStateFlow(StatusUiState())
    val statusUIState: StateFlow<StatusUiState> = _statusUIState.asStateFlow()

    fun getLogs(): StateFlow<List<String>> {
        return loggingRepository.logItemFLow
    }

    fun updateStatus(message: String, running: Boolean) {
        _statusUIState.value = StatusUiState(message, running)
    }

    fun setClient(client: MainServiceClient) {
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