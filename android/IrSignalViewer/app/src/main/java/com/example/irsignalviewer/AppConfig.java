package com.example.irsignalviewer;

public final class AppConfig {
    // ロリポップの初期ドメインに変更してください。
    public static final String API_URL =
            "https://YOUR_LOLIPOP_DOMAIN/ir-signals/api/signals.php?limit=100";

    // server/ir-signals/config.php の viewer_password と同じ値にします。
    public static final String VIEWER_KEY =
            "CHANGE_TO_A_DIFFERENT_VIEWER_PASSWORD";

    private AppConfig() {
    }
}
