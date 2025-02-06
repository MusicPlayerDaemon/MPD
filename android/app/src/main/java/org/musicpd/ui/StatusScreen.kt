package org.musicpd.ui

import android.Manifest
import android.content.Context
import android.os.Build
import android.util.TypedValue
import androidx.annotation.AttrRes
import androidx.annotation.ColorInt
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.text.selection.SelectionContainer
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
import androidx.compose.ui.draw.alpha
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
        ServerStatus(settingsViewModel, storagePermissionState)
        AudioMediaPermission(storagePermissionState)
        MPDLoaderStatus(settingsViewModel)
    }
}

@ColorInt
fun getThemeColorAttribute(context: Context, @AttrRes attr: Int): Int {
    val value = TypedValue()
    if (context.theme.resolveAttribute(attr, value, true)) {
        return value.data
    }
    return android.graphics.Color.BLACK
}

@OptIn(ExperimentalPermissionsApi::class)
@Composable
fun ServerStatus(settingsViewModel: SettingsViewModel, storagePermissionState: PermissionState) {
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
            Row(verticalAlignment = Alignment.CenterVertically) {
                Icon(
                    imageVector = Icons.Default.Circle,
                    contentDescription = "",
                    tint = Color(
                        getThemeColorAttribute(
                            context,
                            if (statusUiState.running) R.attr.appColorPositive else R.attr.appColorNegative
                        )
                    ),
                    modifier = Modifier
                        .padding(end = 8.dp)
                        .alpha(0.6f)
                )
                Text(text = stringResource(id = if (statusUiState.running) R.string.running else R.string.stopped))
            }
            Button(
                onClick = {
                    if (statusUiState.running)
                        settingsViewModel.stopMPD()
                    else
                        settingsViewModel.startMPD(context)
                },
                enabled = settingsViewModel.mpdLoader.isLoaded
                        && storagePermissionState.status.isGranted
            ) {
                Text(
                    text = stringResource(id = if (statusUiState.running) R.string.stopMPD else R.string.startMPD)
                )
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

@Composable
fun MPDLoaderStatus(settingsViewModel: SettingsViewModel) {
    val loader = settingsViewModel.mpdLoader
    if (!loader.isLoaded) {
        val context = LocalContext.current
        SelectionContainer {
            Text(
                loader.loadFailureMessage(context),
                Modifier.padding(16.dp),
                color = MaterialTheme.colorScheme.error
            )
        }
    }
}