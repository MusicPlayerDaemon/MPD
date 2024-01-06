package org.musicpd;

import android.annotation.SuppressLint;
import android.os.Looper;

import androidx.annotation.NonNull;
import androidx.media3.common.Player;
import androidx.media3.common.SimpleBasePlayer;
import androidx.media3.common.util.UnstableApi;

import com.google.common.util.concurrent.Futures;
import com.google.common.util.concurrent.ListenableFuture;

import java.util.Arrays;
import java.util.List;

@UnstableApi
public class MPDPlayer extends SimpleBasePlayer {

    List<MediaItemData> placeholderItems;
    public MPDPlayer(Looper looper) {
        super(looper);

        // Dummy items to let us receive next and previous commands
        MediaItemData item0 = new MediaItemData.Builder(0)
                .build();
        MediaItemData item1 = new MediaItemData.Builder(1)
                .build();
        MediaItemData item2 = new MediaItemData.Builder(2)
                .build();
        MediaItemData[] items = new MediaItemData[] { item0, item1, item2 };

        placeholderItems = Arrays.asList(items);
    }

    @NonNull
    @Override
    protected State getState() {
        Commands commands = new Commands.Builder().addAll(COMMAND_SEEK_TO_PREVIOUS_MEDIA_ITEM, COMMAND_SEEK_TO_NEXT_MEDIA_ITEM).build();

        return new State.Builder()
                .setAvailableCommands(commands)
                .setPlaybackState(Player.STATE_READY)
                .setPlaylist(placeholderItems)
                .setCurrentMediaItemIndex(1)
                .build();
    }

    @NonNull
    @SuppressLint("SwitchIntDef")
    @Override
    protected ListenableFuture<?> handleSeek(int mediaItemIndex, long positionMs, int seekCommand) {
        switch (seekCommand) {
            case COMMAND_SEEK_TO_PREVIOUS_MEDIA_ITEM:
            case COMMAND_SEEK_TO_PREVIOUS:
                Bridge.playPrevious();
                break;
            case COMMAND_SEEK_TO_NEXT_MEDIA_ITEM:
            case COMMAND_SEEK_TO_NEXT:
                Bridge.playNext();
                break;
        }
        return Futures.immediateVoidFuture();
    }
}