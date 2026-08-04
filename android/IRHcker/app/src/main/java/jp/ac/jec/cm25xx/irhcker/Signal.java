package jp.ac.jec.cm25xx.irhcker;

import org.json.JSONObject;

public class Signal {
    public final int id;
    public final String deviceId;
    public String signalName;
    public String manufacturer;
    public boolean favorite;
    public final String protocol;
    public final String signalValue;
    public final int bits;
    public final String receivedAt;

    public Signal(JSONObject json) {
        id = json.optInt("id");
        deviceId = json.optString("device_id", "");
        signalName = json.optString("signal_name", "");
        manufacturer = json.optString("manufacturer", "");
        favorite = json.optBoolean("is_favorite", false);
        protocol = json.optString("protocol", "");
        signalValue = json.optString("signal_value", "");
        bits = json.optInt("bits");
        receivedAt = json.optString("received_at", "");
    }

    public String getDisplayName() {
        return signalName.isEmpty()
                ? "名前未設定の信号 #" + id
                : signalName;
    }

    public String getManufacturerDisplay() {
        return manufacturer.isEmpty()
                ? "メーカー未設定"
                : manufacturer;
    }
}