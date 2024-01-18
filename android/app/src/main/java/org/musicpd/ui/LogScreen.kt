package org.musicpd.ui

import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.lazy.rememberLazyListState
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.State
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.unit.dp
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch

@Composable
fun LogView(messages: State<List<String>>) {
    val state = rememberLazyListState()

    Column(Modifier.fillMaxSize()) {
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
}