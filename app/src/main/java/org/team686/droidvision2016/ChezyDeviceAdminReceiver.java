package org.team686.droidvision2016;

import android.app.admin.DeviceAdminReceiver;
import android.content.ComponentName;
import android.content.Context;

public class ChezyDeviceAdminReceiver extends DeviceAdminReceiver {

    public static ComponentName getComponentName(Context context) {
        return new ComponentName(context.getApplicationContext(), ChezyDeviceAdminReceiver.class);
    }

}