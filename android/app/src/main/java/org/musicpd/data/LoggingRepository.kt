package org.musicpd.data

import android.util.Log
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import javax.inject.Inject
import javax.inject.Singleton

private const val MAX_LOGS = 500

@Singleton
class LoggingRepository @Inject constructor() {

    private val _logItemFLow = MutableStateFlow(listOf<String>())
    val logItemFLow: StateFlow<List<String>> = _logItemFLow

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

}