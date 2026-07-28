# ロリポップ設置・遠隔操作手順

1. ロリポップの「サーバーの管理・設定 > データベース」でMySQLを作成します。
2. phpMyAdminを開き、初回設置なら `schema.sql` をインポートします。以前の版を設置済みなら `migration_add_commands.sql`（未適用の場合）と `migration_add_signal_metadata.sql` を、それぞれ1回インポートします。
3. `config.example.php` を `config.php` にコピーし、DB接続情報、機器用APIキー、閲覧用パスワードを変更します。
4. この `ir-signals` フォルダをロリポップFTPで公開フォルダへアップロードします。
5. M5側の `src/ir_remote_capture/upload_config.example.h` を `upload_config.h` にコピーし、Wi-Fi情報、初期ドメインのAPIベースURL、同じAPIキーを設定して書き込みます。
6. Android Studioで `android/IrSignalViewer` を開き、`AppConfig.java` に初期ドメインの一覧API URLと `viewer_password` を設定します。

課題提出用のため、独自ドメインの取得・設定は不要です。管理画面の「アカウント情報」に表示されるサイトURLを使用してください。

APIキーには32文字以上のランダム文字列を推奨します。`config.php`、各SQLファイル、このREADMEは `.htaccess` で外部アクセスを拒否しています。

## M5設定例

```cpp
constexpr char kWifiSsid[] = "Wi-Fi名";
constexpr char kWifiPassword[] = "Wi-Fiパスワード";
constexpr char kDeviceId[] = "m5stick-living-room";
constexpr char kApiBaseUrl[] =
    "https://アカウント名.lolipop.jp/ir-signals/api";
constexpr char kApiKey[] = "config.phpのapi_keyと同じ値";
```

`kDeviceId` は、信号を登録したM5と遠隔操作するM5を対応付ける識別子です。複数台を使う場合は、M5ごとに異なる値にしてください。

## スマホからの遠隔操作

1. スマホのブラウザで `https://アカウント名.lolipop.jp/ir-signals/` を開きます。
2. `config.php` の `viewer_password` でログインします。
3. 保存済み信号の「この信号をM5から送信」を押します。
4. M5が約1秒ごとに命令を確認し、対象信号を赤外線LEDから送信します。
5. ブラウザを再読み込みし、送信履歴が `completed` になれば完了です。

送信履歴の状態は次の意味です。

- `pending`: M5の取得待ち
- `processing`: M5が命令を取得済み
- `completed`: 赤外線送信と完了報告が成功
- `failed`: M5が命令を実行できなかった

M5の電源が切れている間も命令は `pending` のまま残り、次回オンライン時に古いものから実行されます。

各信号の「信号名・メーカー名を編集」を開くと、「テレビ 電源」のような信号名とメーカー名を別々に保存できます。未設定の既存データも引き続き表示されます。

## Androidアプリ

スマホ側はJava製のAndroidネイティブ閲覧アプリです。遠隔操作には上記のブラウザ画面を使用します。`AppConfig.java` の設定例は次のとおりです。

```java
public static final String API_URL =
        "https://アカウント名.lolipop.jp/ir-signals/api/signals.php";
public static final String VIEWER_KEY = "config.phpのviewer_passwordと同じ値";
```

Android Studioから実機へインストールします。「更新」を押すと保存済みの全信号を取得し、信号名とメーカー名を別々に表示します。各項目をタップするとRAWデータを表示します。名前の編集と削除はブラウザ画面から行います。

ブラウザ画面の「この信号を削除」を押すと、確認後に信号を削除します。外部キーの設定により、その信号に関連する送信履歴も同時に削除されます。

## 動作確認

M5StickC PlusでBボタンを押しながら赤外線を受信します。画面が `Uploaded!` になった後、ブラウザを再読み込みするとデータが表示されます。「この信号をM5から送信」を押し、M5画面が `Sending...`、`Done` と変化することを確認します。失敗時はPlatformIOのシリアルモニター（115200 baud）でHTTPステータスを確認します。

## TLSについて

現状のM5側実装は共有サーバーの証明書差し替えに追従しやすくするため、HTTPSの証明書検証を無効化しています。通信は暗号化されますが、厳密な本番運用では使用中ドメインのルートCAを `WiFiClientSecure::setCACert()` に設定してください。
