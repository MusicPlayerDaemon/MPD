# Android

Notes and resources for MPD android maintainers.

## Build

See [Compiling for Android](https://github.com/MusicPlayerDaemon/MPD/blob/45cb098cd765af12316f8dca5635ef10a852e013/doc/user.rst#compiling-for-android)

## Android studio

### Version control

git ignoring .idea directory completely until a good reason emerges not to

* [How to manage projects under Version Control Systems (jetbrains.com)](https://intellij-support.jetbrains.com/hc/en-us/articles/206544839-How-to-manage-projects-under-Version-Control-Systems)
  
* [gradle.xml should work like workspace.xml? (jetbrains.com)](https://youtrack.jetbrains.com/issue/IDEA-55923)

### Native libraries

*  [Include prebuilt native libraries (developer.android.com)](https://developer.android.com/studio/projects/gradle-external-native-builds#jniLibs)

## Permissions

### Files access

The required permission depends on android SDK version:

    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU)
        Manifest.permission.READ_MEDIA_AUDIO
    else
        Manifest.permission.READ_EXTERNAL_STORAGE

### Permission request

[Request runtime permissions](https://developer.android.com/training/permissions/requesting)

Since Android 6.0 (API level 23):

Android will ignore permission request and will not show the request dialog 
if the user's action implies "don't ask again."
This leaves the app in a crippled state and the user confused.
Google says "don't try to convince the user", so it returns false for `shouldShowRequestPermissionRationale`.

To help the user proceed, we show the `Request permission` button only if `shouldShowRequestPermissionRationale == true`
because there's a good chance the permission request dialog will not be ignored.

If `shouldShowRequestPermissionRationale == false` we instead show the "rationale" message and a button to open
the app info dialog where the user can explicitly grand the permission.