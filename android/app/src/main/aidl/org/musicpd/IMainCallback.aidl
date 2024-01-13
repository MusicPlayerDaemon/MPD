package org.musicpd;

interface IMainCallback
{
    void onStarted();
    void onStopped();
    void onError(String error);
}
