package org.musicpd;

import android.content.Context;
import android.net.ConnectivityManager;
import android.net.LinkAddress;
import android.net.LinkProperties;

import androidx.annotation.Nullable;

import java.net.Inet4Address;
import java.net.InetAddress;
import java.util.List;

public class NetworkUtil {

    @Nullable
    public static String getDeviceIPV4Address(Context context) {
        ConnectivityManager connectivityManager = (ConnectivityManager) context.getSystemService(Context.CONNECTIVITY_SERVICE);
        LinkProperties linkProperties = connectivityManager.getLinkProperties(connectivityManager.getActiveNetwork());
        if (linkProperties != null) {
            List<LinkAddress> linkAddresses = linkProperties.getLinkAddresses();
            for (LinkAddress address : linkAddresses) {
                if (!address.getAddress().isLinkLocalAddress() && !address.getAddress().isLoopbackAddress()) {
                    InetAddress address1 = address.getAddress();
                    if (address1 instanceof Inet4Address) {
                        return address1.getHostAddress();
                    }
                }
            }
        }

        return null;
    }
}
