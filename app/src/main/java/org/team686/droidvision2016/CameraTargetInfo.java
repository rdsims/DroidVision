package org.team686.droidvision2016;

import android.util.Log;

import org.json.JSONException;
import org.json.JSONObject;

public class CameraTargetInfo {
    protected double hAngle;
    protected double vAngle;
    protected double hWidth;
    protected double vWidth;

    // Coordinate frame:
    // +x is out the camera's optical axis
    // +y is to the left of the image
    // +z is to the top of the image
    // hAngle is angle from x-axis along xy plane.  hAngle = pi/2 at y-axis
    // vAngle is angle from x-axis along xz plane.  vAngle = pi/2 at z-axis

    public CameraTargetInfo(double _hAngle, double _vAngle, double _hWidth, double _vWidth)
    {
        hAngle = _hAngle;
        vAngle = _vAngle;
        hWidth = _hWidth;
        vWidth = _vWidth;
    }

    private double doubleize(double value) {
        double leftover = value % 1;
        if (leftover < 1e-7) {
            value += 1e-7;
        }
        return value;
    }

    public JSONObject toJson()
    {
        JSONObject j = new JSONObject();
        try
        {
            j.put("hAngle", doubleize(hAngle));
            j.put("vAngle", doubleize(vAngle));
            j.put("hWidth", doubleize(hWidth));
            j.put("vWidth", doubleize(vWidth));
        }
        catch (JSONException e)
        {
            Log.e("CameraTargetInfo", "Could not encode Json");
        }
        return j;
    }
}