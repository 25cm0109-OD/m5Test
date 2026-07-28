package com.example.irsignalviewer;

import android.app.Activity;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.os.Bundle;
import android.view.Gravity;
import android.view.View;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ProgressBar;
import android.widget.ScrollView;
import android.widget.TextView;

import org.json.JSONArray;
import org.json.JSONObject;

import java.io.BufferedReader;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.net.HttpURLConnection;
import java.net.URL;
import java.nio.charset.StandardCharsets;
import java.time.LocalDateTime;
import java.time.ZoneId;
import java.time.ZoneOffset;
import java.time.format.DateTimeFormatter;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

public class MainActivity extends Activity {
    private static final int COLOR_BACKGROUND = Color.rgb(11, 16, 32);
    private static final int COLOR_CARD = Color.rgb(20, 29, 52);
    private static final int COLOR_MUTED = Color.rgb(150, 166, 196);
    private static final int COLOR_ACCENT = Color.rgb(56, 189, 248);

    private final ExecutorService executor = Executors.newSingleThreadExecutor();
    private LinearLayout signalList;
    private TextView statusText;
    private ProgressBar progressBar;
    private Button refreshButton;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(createContentView());
        loadSignals();
    }

    private View createContentView() {
        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setPadding(dp(18), dp(22), dp(18), 0);
        root.setBackgroundColor(COLOR_BACKGROUND);

        LinearLayout header = new LinearLayout(this);
        header.setGravity(Gravity.CENTER_VERTICAL);

        TextView title = text("赤外線信号ログ", 25, Color.WHITE);
        title.setTypeface(Typeface.DEFAULT, Typeface.BOLD);
        header.addView(title, new LinearLayout.LayoutParams(0, dp(52), 1));

        refreshButton = new Button(this);
        refreshButton.setText("更新");
        refreshButton.setTextColor(Color.rgb(6, 34, 56));
        refreshButton.setBackground(rounded(COLOR_ACCENT, 10));
        refreshButton.setOnClickListener(view -> loadSignals());
        header.addView(refreshButton, new LinearLayout.LayoutParams(dp(82), dp(46)));
        root.addView(header);

        LinearLayout statusRow = new LinearLayout(this);
        statusRow.setGravity(Gravity.CENTER_VERTICAL);
        statusRow.setPadding(0, dp(4), 0, dp(12));
        progressBar = new ProgressBar(this, null, android.R.attr.progressBarStyleSmall);
        statusRow.addView(progressBar, new LinearLayout.LayoutParams(dp(28), dp(28)));
        statusText = text("読み込み中…", 14, COLOR_MUTED);
        statusRow.addView(statusText);
        root.addView(statusRow);

        ScrollView scrollView = new ScrollView(this);
        scrollView.setFillViewport(true);
        signalList = new LinearLayout(this);
        signalList.setOrientation(LinearLayout.VERTICAL);
        signalList.setPadding(0, 0, 0, dp(30));
        scrollView.addView(signalList);
        root.addView(scrollView, new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT, 0, 1));
        return root;
    }

    private void loadSignals() {
        refreshButton.setEnabled(false);
        progressBar.setVisibility(View.VISIBLE);
        statusText.setText("読み込み中…");

        executor.execute(() -> {
            try {
                JSONArray signals = requestSignals();
                runOnUiThread(() -> renderSignals(signals));
            } catch (Exception exception) {
                runOnUiThread(() -> showError(exception.getMessage()));
            }
        });
    }

    private JSONArray requestSignals() throws Exception {
        HttpURLConnection connection = (HttpURLConnection) new URL(AppConfig.API_URL).openConnection();
        connection.setRequestMethod("GET");
        connection.setRequestProperty("Accept", "application/json");
        connection.setRequestProperty("X-Viewer-Key", AppConfig.VIEWER_KEY);
        connection.setConnectTimeout(10000);
        connection.setReadTimeout(10000);

        int status = connection.getResponseCode();
        InputStream stream = status >= 200 && status < 300
                ? connection.getInputStream() : connection.getErrorStream();
        String response = readAll(stream);
        connection.disconnect();

        if (status != HttpURLConnection.HTTP_OK) {
            throw new IllegalStateException("通信エラー（HTTP " + status + "）");
        }
        JSONObject root = new JSONObject(response);
        if (!root.optBoolean("ok")) {
            throw new IllegalStateException("サーバーからデータを取得できませんでした");
        }
        return root.getJSONArray("signals");
    }

    private String readAll(InputStream stream) throws Exception {
        if (stream == null) {
            return "";
        }
        StringBuilder result = new StringBuilder();
        try (BufferedReader reader = new BufferedReader(
                new InputStreamReader(stream, StandardCharsets.UTF_8))) {
            String line;
            while ((line = reader.readLine()) != null) {
                result.append(line);
            }
        }
        return result.toString();
    }

    private void renderSignals(JSONArray signals) {
        signalList.removeAllViews();
        progressBar.setVisibility(View.GONE);
        refreshButton.setEnabled(true);
        statusText.setText("最新 " + signals.length() + " 件");

        if (signals.length() == 0) {
            TextView empty = text("受信データはまだありません。", 16, COLOR_MUTED);
            empty.setGravity(Gravity.CENTER);
            signalList.addView(empty, new LinearLayout.LayoutParams(
                    LinearLayout.LayoutParams.MATCH_PARENT, dp(150)));
            return;
        }

        for (int index = 0; index < signals.length(); index++) {
            JSONObject signal = signals.optJSONObject(index);
            if (signal != null) {
                signalList.addView(createSignalCard(signal));
            }
        }
    }

    private View createSignalCard(JSONObject signal) {
        LinearLayout card = new LinearLayout(this);
        card.setOrientation(LinearLayout.VERTICAL);
        card.setPadding(dp(16), dp(15), dp(16), dp(15));
        card.setBackground(rounded(COLOR_CARD, 16));

        TextView protocol = text(signal.optString("protocol", "UNKNOWN"), 13, Color.CYAN);
        protocol.setTypeface(Typeface.DEFAULT, Typeface.BOLD);
        card.addView(protocol);

        TextView device = text(signal.optString("device_id", "-"), 19, Color.WHITE);
        device.setTypeface(Typeface.DEFAULT, Typeface.BOLD);
        device.setPadding(0, dp(5), 0, dp(3));
        card.addView(device);
        card.addView(text(formatTime(signal.optString("received_at", "")), 13, COLOR_MUTED));

        String summary = "値  " + signal.optString("signal_value", "-")
                + "\nAddress  " + signal.optString("address_value", "-")
                + "    Command  " + signal.optString("command_value", "-")
                + "\nBits  " + signal.optString("bits", "-")
                + "    Carrier  " + signal.optString("carrier_khz", "-") + " kHz"
                + "    RAW  " + signal.optString("raw_length", "-");
        TextView values = text(summary, 14, Color.rgb(220, 229, 246));
        values.setTypeface(Typeface.MONOSPACE);
        values.setPadding(0, dp(12), 0, dp(8));
        card.addView(values);

        TextView raw = text(signal.optJSONArray("raw_data") != null
                ? signal.optJSONArray("raw_data").toString() : "[]", 12, COLOR_MUTED);
        raw.setTypeface(Typeface.MONOSPACE);
        raw.setVisibility(View.GONE);
        card.addView(raw);

        TextView toggle = text("RAWデータを表示", 13, COLOR_ACCENT);
        toggle.setPadding(0, dp(5), 0, 0);
        card.addView(toggle);
        card.setOnClickListener(view -> {
            boolean opening = raw.getVisibility() != View.VISIBLE;
            raw.setVisibility(opening ? View.VISIBLE : View.GONE);
            toggle.setText(opening ? "RAWデータを閉じる" : "RAWデータを表示");
        });

        LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                LinearLayout.LayoutParams.WRAP_CONTENT);
        params.bottomMargin = dp(13);
        card.setLayoutParams(params);
        return card;
    }

    private String formatTime(String utcText) {
        try {
            DateTimeFormatter input = DateTimeFormatter.ofPattern("yyyy-MM-dd HH:mm:ss");
            LocalDateTime utc = LocalDateTime.parse(utcText, input);
            return utc.atZone(ZoneOffset.UTC)
                    .withZoneSameInstant(ZoneId.systemDefault())
                    .format(DateTimeFormatter.ofPattern("yyyy/MM/dd HH:mm:ss"));
        } catch (Exception ignored) {
            return utcText;
        }
    }

    private void showError(String message) {
        progressBar.setVisibility(View.GONE);
        refreshButton.setEnabled(true);
        statusText.setText(message == null ? "読み込みに失敗しました" : message);
    }

    private TextView text(String value, float sizeSp, int color) {
        TextView view = new TextView(this);
        view.setText(value);
        view.setTextSize(sizeSp);
        view.setTextColor(color);
        return view;
    }

    private GradientDrawable rounded(int color, int radiusDp) {
        GradientDrawable drawable = new GradientDrawable();
        drawable.setColor(color);
        drawable.setCornerRadius(dp(radiusDp));
        return drawable;
    }

    private int dp(int value) {
        return Math.round(value * getResources().getDisplayMetrics().density);
    }

    @Override
    protected void onDestroy() {
        executor.shutdownNow();
        super.onDestroy();
    }
}
