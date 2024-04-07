package org.musicpd.ui

import MPDSettings
import android.graphics.drawable.Icon
import androidx.compose.foundation.layout.padding
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Circle
import androidx.compose.material.icons.filled.Home
import androidx.compose.material.icons.filled.List
import androidx.compose.material.icons.filled.Settings
import androidx.compose.material3.Icon
import androidx.compose.material3.NavigationBar
import androidx.compose.material3.NavigationBarItem
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import androidx.lifecycle.viewmodel.compose.viewModel
import androidx.navigation.NavController
import androidx.navigation.NavHostController
import androidx.navigation.compose.NavHost
import androidx.navigation.compose.composable
import androidx.navigation.compose.currentBackStackEntryAsState
import androidx.navigation.compose.rememberNavController

enum class Screen {
    HOME,
    LOGS,
    SETTINGS,
}
sealed class NavigationItem(val route: String, val label: String, val icon: ImageVector) {
    data object Home : NavigationItem(
        Screen.HOME.name,
        "Home",
        Icons.Default.Home
        )
    data object Logs : NavigationItem(
        Screen.LOGS.name,
        "Logs",
        Icons.Default.List)
    data object Settings : NavigationItem(
        Screen.SETTINGS.name,
        "Settings",
        Icons.Default.Settings)
}

@Composable
fun MPDApp(
    navController: NavHostController = rememberNavController(),
    settingsViewModel: SettingsViewModel = viewModel()
) {
    Scaffold(
        topBar = {

        },
        bottomBar = {
            BottomNavigationBar(navController)
        },
    ) { innerPadding ->
        NavHost(
            navController = navController,
            startDestination = NavigationItem.Home.route,
            modifier = Modifier.padding(innerPadding)
        ) {
            composable(NavigationItem.Home.route) {
                StatusScreen(settingsViewModel)
            }
            composable(NavigationItem.Logs.route) {
                LogView(settingsViewModel.getLogs().collectAsStateWithLifecycle().value)
            }
            composable(NavigationItem.Settings.route) {
                MPDSettings(settingsViewModel)
            }
        }
    }
}

@Composable
fun BottomNavigationBar(navController: NavController) {
    val navBackStackEntry by navController.currentBackStackEntryAsState()
    val currentRoute = navBackStackEntry?.destination?.route

    val items = listOf(
        NavigationItem.Home,
        NavigationItem.Logs,
        NavigationItem.Settings,
    )

    NavigationBar {
        items.forEach { item ->
            NavigationBarItem(
                icon = {
                    Icon(
                        imageVector = item.icon,
                        contentDescription = null
                    )
                },
                label = { Text (item.label) },
                onClick = {
                    navController.navigate(item.route) {
                        popUpTo(navController.graph.startDestinationId)
                        launchSingleTop = true
                    }
                },
                selected = currentRoute == item.route,
            )
        }
    }
}