package org.musicpd.ui

import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.lazy.rememberLazyListState
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.VerticalAlignBottom
import androidx.compose.material3.FloatingActionButton
import androidx.compose.material3.Icon
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.snapshotFlow
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch

@Composable
fun LogView(messages: List<String>) {
    val lazyListState = rememberLazyListState()

    var userScrolled = remember { mutableStateOf(false) }

    LaunchedEffect(lazyListState) {
        snapshotFlow { lazyListState.isScrollInProgress }
            .collect {
                if (it) {
                    userScrolled.value = true
                }
            }
    }

    Box(Modifier.fillMaxSize()) {
        LazyColumn(
            Modifier.padding(4.dp),
            lazyListState
        ) {
            items(messages) { message ->
                Text(text = message, fontFamily = FontFamily.Monospace)
            }
            CoroutineScope(Dispatchers.Main).launch {
                lazyListState.scrollToItem(messages.count(), 0)
            }
        }

        if (lazyListState.canScrollForward) {
            FloatingActionButton(
                onClick = {
                    userScrolled.value = false
                    CoroutineScope(Dispatchers.Main).launch {
                        lazyListState.scrollToItem(messages.count(), 0)
                    }
                },
                modifier = Modifier.padding(16.dp).align(Alignment.BottomEnd)
            ) {
                Icon(Icons.Filled.VerticalAlignBottom, "Scroll to bottom icon")
            }
        }
    }
}

@Preview
@Composable
fun LogViewPreview() {
    val data = listOf("test",
        "test2",
        "test3")
    LogView(data)
}
