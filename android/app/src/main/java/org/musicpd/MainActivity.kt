package org.musicpd

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.viewModels
import androidx.compose.material3.MaterialTheme
import androidx.core.view.WindowCompat
import dagger.hilt.android.AndroidEntryPoint
import org.musicpd.ui.MPDApp
import org.musicpd.ui.SettingsViewModel

@AndroidEntryPoint
class MainActivity : ComponentActivity() {
    private val settingsViewModel: SettingsViewModel by viewModels()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        WindowCompat.setDecorFitsSystemWindows(window, false)
        setContent {
            MaterialTheme {
                MPDApp(settingsViewModel = settingsViewModel)
            }
        }
    }

    private fun connectClient() {
        val client = Main.Client(this, object : Main.Client.Callback {
            override fun onStopped() {
                settingsViewModel.updateStatus("", false)
            }

            override fun onStarted() {
                settingsViewModel.updateStatus("MPD Service Started", true)
            }

            override fun onError(error: String) {
                settingsViewModel.removeClient()
                settingsViewModel.updateStatus(error, false)
                connectClient()
            }
        })

        settingsViewModel.setClient(client)
    }

    override fun onStart() {
        //mFirstRun = false
        connectClient()
        super.onStart()
    }

    override fun onStop() {
        settingsViewModel.removeClient()
        super.onStop()
    }
}
