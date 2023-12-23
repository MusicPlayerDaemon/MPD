package org.musicpd.ui

import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.BatteryAlert
import androidx.compose.material.icons.filled.Headphones
import androidx.compose.material.icons.filled.PowerSettingsNew
import androidx.compose.material3.Icon
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.res.stringResource
import com.alorma.compose.settings.storage.preferences.rememberPreferenceBooleanSettingState
import com.alorma.compose.settings.ui.SettingsSwitch
import org.musicpd.Preferences
import org.musicpd.R

@Composable
fun SettingsOptions(
    onBootChanged: (Boolean) -> Unit,
    onWakeLockChanged: (Boolean) -> Unit,
    onHeadphonesChanged: (Boolean) -> Unit
) {
    val bootState = rememberPreferenceBooleanSettingState(
        key = Preferences.KEY_RUN_ON_BOOT,
        defaultValue = false
    )
    val wakelockState =
        rememberPreferenceBooleanSettingState(key = Preferences.KEY_WAKELOCK, defaultValue = false)
    val headphoneState = rememberPreferenceBooleanSettingState(
        key = Preferences.KEY_PAUSE_ON_HEADPHONES_DISCONNECT,
        defaultValue = false
    )

    SettingsSwitch(
        icon = { Icon(imageVector = Icons.Default.PowerSettingsNew, contentDescription = "Power") },
        title = { Text(text = stringResource(R.string.checkbox_run_on_boot)) },
        onCheckedChange = onBootChanged,
        state = bootState
    )
    SettingsSwitch(
        icon = { Icon(imageVector = Icons.Default.BatteryAlert, contentDescription = "Battery") },
        title = { Text(text = stringResource(R.string.checkbox_wakelock)) },
        onCheckedChange = onWakeLockChanged,
        state = wakelockState
    )
    SettingsSwitch(
        icon = { Icon(imageVector = Icons.Default.Headphones, contentDescription = "Headphones") },
        title = { Text(text = stringResource(R.string.checkbox_pause_on_headphones_disconnect)) },
        onCheckedChange = onHeadphonesChanged,
        state = headphoneState
    )

}
