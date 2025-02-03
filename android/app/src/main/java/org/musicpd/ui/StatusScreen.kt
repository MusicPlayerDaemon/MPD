package org.musicpd.ui

import android.Manifest
import android.os.Build
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Circle
import androidx.compose.material3.Button
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import com.google.accompanist.permissions.ExperimentalPermissionsApi
import com.google.accompanist.permissions.PermissionState
import com.google.accompanist.permissions.isGranted
import com.google.accompanist.permissions.rememberPermissionState
import com.google.accompanist.permissions.shouldShowRationale
import org.musicpd.R
import org.musicpd.utils.openAppSettings

@OptIn(ExperimentalPermissionsApi::class)
@Composable
fun StatusScreen(settingsViewModel: SettingsViewModel) {
    val storagePermissionState = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
        rememberPermissionState(
            Manifest.permission.READ_MEDIA_AUDIO
        )
    } else {
        rememberPermissionState(
            Manifest.permission.READ_EXTERNAL_STORAGE
        )
    }

    Column(
        Modifier
            .padding(4.dp)
            .fillMaxSize(),
        horizontalAlignment = Alignment.CenterHorizontally,
        verticalArrangement = Arrangement.Center
    ) {
        NetworkAddress()
        ServerStatus(settingsViewModel)
        AudioMediaPermission(storagePermissionState)
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

@OptIn(ExperimentalPermissionsApi::class)
@Composable
fun AudioMediaPermission(storagePermissionState: PermissionState) {
    val permissionStatus = storagePermissionState.status
    if (!permissionStatus.isGranted) {
        val context = LocalContext.current
        Column(
            Modifier
                .padding(4.dp)
                .fillMaxWidth(),
            horizontalAlignment = Alignment.CenterHorizontally,
            verticalArrangement = Arrangement.Center
        ) {
            Text(
                stringResource(id = R.string.external_files_permission_request),
                Modifier.padding(16.dp)
            )
            if (storagePermissionState.status.shouldShowRationale) {
                Button(onClick = {
                    storagePermissionState.launchPermissionRequest()
                }) {
                    Text("Request permission")
                }
            } else {
                OutlinedButton(
                    onClick = {
                        openAppSettings(context, context.packageName)
                    },
                    Modifier.padding(16.dp)
                ) {
                    Text(
                        stringResource(id = R.string.title_open_app_info),
                        color = MaterialTheme.colorScheme.secondary
                    )
                }
            }
        }
    }
}