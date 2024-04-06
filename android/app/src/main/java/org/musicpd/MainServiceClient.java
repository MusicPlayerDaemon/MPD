package org.musicpd;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.IBinder;
import android.os.RemoteException;


/*
 * Client that bind the Main Service in order to send commands and receive callback
 */
public class MainServiceClient {

    private static final String REMOTE_ERROR = "MPD process was killed";

        public interface Callback {
            public void onStarted();
            public void onStopped();
            public void onError(String error);
        }

        private boolean mBound = false;
        private final Context mContext;
        private Callback mCallback;
        private IMain mIMain = null;

        private final IMainCallback.Stub mICallback = new IMainCallback.Stub() {

            @Override
            public void onStopped() throws RemoteException {
                mCallback.onStopped();
            }

            @Override
            public void onStarted() throws RemoteException {
                mCallback.onStarted();
            }

            @Override
            public void onError(String error) throws RemoteException {
                mCallback.onError(error);
            }
        };

        private final ServiceConnection mServiceConnection = new ServiceConnection() {

            @Override
            public void onServiceConnected(ComponentName name, IBinder service) {
                synchronized (this) {
                    mIMain = IMain.Stub.asInterface(service);
                    try {
                        if (mCallback != null)
                            mIMain.registerCallback(mICallback);
                    } catch (RemoteException e) {
                        if (mCallback != null)
                            mCallback.onError(REMOTE_ERROR);
                    }
                }
            }

            @Override
            public void onServiceDisconnected(ComponentName name) {
                if (mCallback != null)
                    mCallback.onError(REMOTE_ERROR);
            }
        };

    public MainServiceClient(Context context, Callback cb) throws IllegalArgumentException {
        if (context == null)
            throw new IllegalArgumentException("Context can't be null");
        mContext = context;
        mCallback = cb;
        mBound = mContext.bindService(new Intent(mContext, Main.class), mServiceConnection, Context.BIND_AUTO_CREATE);
    }

    public boolean start() {
        synchronized (this) {
            if (mIMain != null) {
                try {
                    mIMain.start();
                    return true;
                } catch (RemoteException e) {
                }
            }
            return false;
        }
    }

    public boolean stop() {
        synchronized (this) {
            if (mIMain != null) {
                try {
                    mIMain.stop();
                    return true;
                } catch (RemoteException e) {
                }
            }
            return false;
        }
    }

    public boolean setPauseOnHeadphonesDisconnect(boolean enabled) {
        synchronized (this) {
            if (mIMain != null) {
                try {
                    mIMain.setPauseOnHeadphonesDisconnect(enabled);
                    return true;
                } catch (RemoteException e) {
                }
            }
            return false;
        }
    }

    public boolean setWakelockEnabled(boolean enabled) {
        synchronized (this) {
            if (mIMain != null) {
                try {
                    mIMain.setWakelockEnabled(enabled);
                    return true;
                } catch (RemoteException e) {
                }
            }
            return false;
        }
    }

    public boolean isRunning() {
        synchronized (this) {
            if (mIMain != null) {
                try {
                    return mIMain.isRunning();
                } catch (RemoteException e) {
                }
            }
            return false;
        }
    }

    public void release() {
        if (mBound) {
            synchronized (this) {
                if (mIMain != null && mICallback != null) {
                    try {
                        if (mCallback != null)
                            mIMain.unregisterCallback(mICallback);
                    } catch (RemoteException e) {
                    }
                }
            }
            mBound = false;
            mContext.unbindService(mServiceConnection);
        }
    }
}
