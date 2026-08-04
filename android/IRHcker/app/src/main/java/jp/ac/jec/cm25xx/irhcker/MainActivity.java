package jp.ac.jec.cm25xx.irhcker;


import android.app.AlertDialog;
import android.os.Bundle;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.ListView;
import android.widget.ProgressBar;
import android.widget.TextView;
import android.widget.Toast;

import androidx.activity.EdgeToEdge;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.graphics.Insets;
import androidx.core.view.ViewCompat;
import androidx.core.view.WindowInsetsCompat;

import org.json.JSONArray;
import org.json.JSONObject;

import java.io.BufferedReader;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

public class MainActivity extends AppCompatActivity {

    private Button loadButton;
    private ProgressBar progressBar;
    private TextView statusText;


    private ListView signalListView;
    private final List<Signal> signalList = new ArrayList<>();
    private SignalAdapter signalAdapter;

    private final ExecutorService executor =
            Executors.newSingleThreadExecutor();

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        EdgeToEdge.enable(this);
        setContentView(R.layout.activity_main);

        loadButton = findViewById(R.id.loadButton);
        progressBar = findViewById(R.id.progressBar);
        statusText = findViewById(R.id.statusText);
        signalListView = findViewById(R.id.signalListView);

        TextView emptyText = findViewById(R.id.emptyText);
        signalListView.setEmptyView(emptyText);

        loadButton.setOnClickListener(v -> loadSignals());

        signalListView = findViewById(R.id.signalListView);

        signalAdapter = new SignalAdapter(
                this,
                signalList,
                new SignalAdapter.Listener() {
                    @Override
                    public void onSend(Signal signal) {
                        sendSignal(signal);
                    }

                    @Override
                    public void onEdit(Signal signal) {
                        showEditDialog(signal);
                    }

                    @Override
                    public void onFavorite(Signal signal) {
                        updateFavorite(signal);
                    }
                }
        );

        signalListView.setAdapter(signalAdapter);


        ViewCompat.setOnApplyWindowInsetsListener(findViewById(R.id.main), (v, insets) -> {
            Insets systemBars = insets.getInsets(WindowInsetsCompat.Type.systemBars());
            v.setPadding(systemBars.left, systemBars.top, systemBars.right, systemBars.bottom);
            return insets;
        });
    }
    private void sendSignal(Signal signal) {
        statusText.setText(signal.getDisplayName() + " を送信予約中...");

        executor.execute(() -> {
            try {
                JSONObject body = new JSONObject();
                body.put("signal_id", signal.id);

                postJson(AppConfig.SEND_URL, body);

                runOnUiThread(() -> {
                    statusText.setText(
                            signal.getDisplayName() + " をM5へ送信予約しました"
                    );
                    Toast.makeText(
                            this,
                            "送信予約しました",
                            Toast.LENGTH_SHORT
                    ).show();
                });
            } catch (Exception exception) {
                runOnUiThread(() -> showError(exception));
            }
        });
    }

    //ボタンを押したらサーバーからロードする>>>requestSignalsを取得>>> それをshowSIgnals()へ
    private void loadSignals() {
        loadButton.setEnabled(false);
        progressBar.setVisibility(View.VISIBLE);
        statusText.setText("LOADING...");

        executor.execute(() -> {
            try {
                JSONArray signals = requestSignals();
                runOnUiThread(() -> showSignals(signals));
            } catch (Exception exception) {
                runOnUiThread(() -> showError(exception));
            }
        });
    }
    //HttpURLConnctionでURLに接続
    private JSONArray requestSignals() throws Exception {
        URL url = new URL(AppConfig.API_URL);

        HttpURLConnection connection = (HttpURLConnection) url.openConnection();

        try {
            connection.setRequestMethod("GET");
            connection.setRequestProperty(
                    "Accept",
                    "application/json"
            );
            connection.setRequestProperty(
                    "X-Viewer-Key",
                    AppConfig.VIEWER_KEY
            );

            connection.setConnectTimeout(10000);
            connection.setReadTimeout(10000);

            int statusCode = connection.getResponseCode();

            InputStream inputStream;

            if (statusCode >= 200 && statusCode < 300) {
                inputStream = connection.getInputStream();
            } else {
                inputStream = connection.getErrorStream();
            }

            String response = readAll(inputStream);

            if (statusCode != 200) {
                throw new Exception(
                        "HTTPエラー:" + statusCode + "\n" + response
                );
            }
            JSONObject root = new JSONObject(response);

            if (!root.optBoolean("ok", false)) {
                throw new Exception("APIエラー");
            }

            return root.optJSONArray("signals");
        }finally {
            connection.disconnect();
        }


    }

    private String readAll(InputStream inputStream)throws Exception{
        if (inputStream == null) {
            return "";
        }
        StringBuilder builder = new StringBuilder();

        try (BufferedReader reader = new BufferedReader(
                new InputStreamReader(inputStream,
                        StandardCharsets.UTF_8
                ))) {
            String line;
            while ((line = reader.readLine()) != null) {
                builder.append(line);
            }
        }
        return builder.toString();


    }
    private void showSignals(JSONArray jsonSignals) {
        signalList.clear();

        for (int i = 0; i < jsonSignals.length(); i++) {
            JSONObject json = jsonSignals.optJSONObject(i);

            if (json != null) {
                signalList.add(new Signal(json));
            }
        }

        signalAdapter.notifyDataSetChanged();

        statusText.setText(signalList.size() + "件取得しました");
        progressBar.setVisibility(View.GONE);
        loadButton.setEnabled(true);
    }

    private JSONObject postJson(String urlString, JSONObject body)
            throws Exception {

        HttpURLConnection connection =
                (HttpURLConnection) new URL(urlString).openConnection();

        try {
            connection.setRequestMethod("POST");
            connection.setDoOutput(true);
            connection.setConnectTimeout(10000);
            connection.setReadTimeout(10000);

            connection.setRequestProperty(
                    "Content-Type",
                    "application/json; charset=UTF-8"
            );
            connection.setRequestProperty(
                    "Accept",
                    "application/json"
            );
            connection.setRequestProperty(
                    "X-Viewer-Key",
                    AppConfig.VIEWER_KEY
            );

            byte[] requestBody =
                    body.toString().getBytes(StandardCharsets.UTF_8);

            try (OutputStream output = connection.getOutputStream()) {
                output.write(requestBody);
            }

            int statusCode = connection.getResponseCode();

            InputStream input = statusCode >= 200 && statusCode < 300
                    ? connection.getInputStream()
                    : connection.getErrorStream();

            String response = readAll(input);

            if (statusCode < 200 || statusCode >= 300) {
                throw new Exception(
                        "HTTPエラー: " + statusCode + "\n" + response
                );
            }

            JSONObject result = new JSONObject(response);

            if (!result.optBoolean("ok", false)) {
                throw new Exception("API処理に失敗しました");
            }

            return result;
        } finally {
            connection.disconnect();
        }
    }

    private void showEditDialog(Signal signal) {
        LinearLayout layout = new LinearLayout(this);
        layout.setOrientation(LinearLayout.VERTICAL);

        int padding = (int) (20 * getResources().getDisplayMetrics().density);
        layout.setPadding(padding, 0, padding, 0);

        EditText nameInput = new EditText(this);
        nameInput.setHint("信号名 例: テレビ 電源");
        nameInput.setText(signal.signalName);

        EditText manufacturerInput = new EditText(this);
        manufacturerInput.setHint("メーカー名 例: Panasonic");
        manufacturerInput.setText(signal.manufacturer);

        layout.addView(nameInput);
        layout.addView(manufacturerInput);

        new AlertDialog.Builder(this)
                .setTitle("信号情報を編集")
                .setView(layout)
                .setNegativeButton("キャンセル", null)
                .setPositiveButton("保存", (dialog, which) -> {
                    updateSignal(
                            signal,
                            nameInput.getText().toString().trim(),
                            manufacturerInput.getText().toString().trim(),
                            signal.favorite
                    );
                })
                .show();
    }



    private void updateFavorite(Signal signal) {
        updateSignal(
                signal,
                signal.signalName,
                signal.manufacturer,
                !signal.favorite
        );
    }

    private void updateSignal(
            Signal signal,
            String name,
            String manufacturer,
            boolean favorite
    ) {
        executor.execute(() -> {
            try {
                JSONObject body = new JSONObject();
                body.put("signal_id", signal.id);
                body.put("signal_name", name);
                body.put("manufacturer", manufacturer);
                body.put("is_favorite", favorite);

                postJson(AppConfig.UPDATE_URL, body);

                runOnUiThread(() -> {
                    signal.signalName = name;
                    signal.manufacturer = manufacturer;
                    signal.favorite = favorite;

                    signalAdapter.notifyDataSetChanged();
                    statusText.setText("信号情報を保存しました");

                    // お気に入りを上に並べ直す
                    loadSignals();
                });
            } catch (Exception exception) {
                runOnUiThread(() -> showError(exception));
            }
        });
    }

    private void showError(Exception exception) {
        String message = exception.getMessage();

        if (message == null || message.isEmpty()) {
            message = "不明なエラー";
        }

        statusText.setText("処理に失敗: " + message);

        Toast.makeText(
                this,
                message,
                Toast.LENGTH_LONG
        ).show();

        progressBar.setVisibility(View.GONE);
        loadButton.setEnabled(true);
    }


    protected void onDestroy() {
        executor.shutdownNow();
        super.onDestroy();
    }

}