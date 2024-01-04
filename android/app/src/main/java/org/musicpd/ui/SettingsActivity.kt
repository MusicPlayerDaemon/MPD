package org.musicpd.ui

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.viewModels
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.lazy.rememberLazyListState
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Circle
import androidx.compose.material3.Button
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.State
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import androidx.lifecycle.viewmodel.compose.viewModel
import com.google.accompanist.permissions.ExperimentalPermissionsApi
import com.google.accompanist.permissions.isGranted
import com.google.accompanist.permissions.rememberPermissionState
import com.google.accompanist.permissions.shouldShowRationale
import dagger.hilt.android.AndroidEntryPoint
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import org.musicpd.Main
import org.musicpd.R

@AndroidEntryPoint
class SettingsActivity : ComponentActivity() {

    private val settingsViewModel: SettingsViewModel by viewModels()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        setContent {
            MaterialTheme {
                SettingsContainer(settingsViewModel)
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

@OptIn(ExperimentalPermissionsApi::class)
@Composable
fun SettingsContainer(settingsViewModel: SettingsViewModel = viewModel()) {
    val context = LocalContext.current

    val storagePermissionState = rememberPermissionState(
        android.Manifest.permission.READ_EXTERNAL_STORAGE
    )

    if (storagePermissionState.status.shouldShowRationale) {
        Column(Modifier
            .padding(4.dp)
            .fillMaxWidth(),
            horizontalAlignment = Alignment.CenterHorizontally,
            verticalArrangement = Arrangement.Center
        ) {
            Text(stringResource(id = R.string.external_files_permission_request))
            Button(onClick = {  }) {
                Text("Request permission")
            }
        }
    } else {
        Column {
            NetworkAddress()
            ServerStatus(settingsViewModel)
            if (!storagePermissionState.status.isGranted) {
                OutlinedButton(onClick = { storagePermissionState.launchPermissionRequest() }, Modifier
                    .padding(4.dp)
                    .fillMaxWidth()) {
                    Text("Request external storage permission", color = MaterialTheme.colorScheme.secondary)
                }
            }
            SettingsOptions(
                onBootChanged = { newValue ->
                    if (newValue) {
                        settingsViewModel.startMPD(context)
                    }
                },
                onWakeLockChanged = { newValue ->
                    settingsViewModel.setWakelockEnabled(newValue)
                },
                onHeadphonesChanged = { newValue ->
                    settingsViewModel.setPauseOnHeadphonesDisconnect(newValue)
                }
            )
            LogView(settingsViewModel.getLogs().collectAsStateWithLifecycle())
        }
    }
}

@Composable
fun ServerStatus(settingsViewModel: SettingsViewModel) {
    val context = LocalContext.current

    val statusUiState by settingsViewModel.statusUIState.collectAsState()

    Column {
        Row(
            Modifier
                .padding(4.dp)
                .fillMaxWidth(),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.SpaceEvenly
        ) {
            Row {
                Icon(
                    imageVector = Icons.Default.Circle,
                    contentDescription = "",
                    tint = if (statusUiState.running) Color(0xFFB8F397) else Color(0xFFFFDAD6)
                )
                Text(text = if (statusUiState.running) "Running" else "Stopped")
            }
            Button(onClick = {
                if (statusUiState.running)
                    settingsViewModel.stopMPD()
                else
                    settingsViewModel.startMPD(context)
            }) {
                Text(text = if (statusUiState.running) "Stop MPD" else "Start MPD")
            }
        }
        Row(
            Modifier
                .padding(4.dp)
                .fillMaxWidth(),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.SpaceEvenly
        ) {
            Text(text = statusUiState.statusMessage)
        }
    }
}

@Composable
fun LogView(messages: State<List<String>>) {
    val state = rememberLazyListState()

    LazyColumn(
        Modifier.padding(4.dp),
        state
    ) {
        items(messages.value) { message ->
            Text(text = message, fontFamily = FontFamily.Monospace)
        }
        CoroutineScope(Dispatchers.Main).launch {
            state.scrollToItem(messages.value.count(), 0)
        }
    }
}

@Preview(showBackground = true)
@Composable
fun SettingsPreview() {
    MaterialTheme {
        SettingsContainer()
    }
}