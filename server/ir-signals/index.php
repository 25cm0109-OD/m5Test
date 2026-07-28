<?php
declare(strict_types=1);

require __DIR__ . '/common.php';

$config = appConfig();
date_default_timezone_set((string) $config['timezone']);
session_name('ir_signal_viewer');
session_set_cookie_params([
    'httponly' => true,
    'secure' => true,
    'samesite' => 'Strict',
]);
session_start();

$loginError = false;
if ($_SERVER['REQUEST_METHOD'] === 'POST') {
    $action = (string) ($_POST['action'] ?? '');

    if ($action === 'logout') {
        $_SESSION = [];
        session_destroy();
        header('Location: ./');
        exit;
    }

    if ($action === 'login') {
        $password = (string) ($_POST['password'] ?? '');
        if (hash_equals((string) $config['viewer_password'], $password)) {
            session_regenerate_id(true);
            $_SESSION['authenticated'] = true;
            header('Location: ./');
            exit;
        }
        $loginError = true;
    }

    if ($action === 'enqueue' &&
        ($_SESSION['authenticated'] ?? false) === true) {
        $csrfToken = (string) ($_POST['csrf_token'] ?? '');
        $sessionToken = (string) ($_SESSION['csrf_token'] ?? '');
        $signalId = filter_var(
            $_POST['signal_id'] ?? null,
            FILTER_VALIDATE_INT,
            ['options' => ['min_range' => 1]]
        );

        if ($sessionToken === '' || !hash_equals($sessionToken, $csrfToken) ||
            $signalId === false) {
            $_SESSION['flash'] = [
                'type' => 'error',
                'message' => '送信要求を確認できませんでした。もう一度お試しください。',
            ];
        } else {
            $pdo = db();
            try {
                $pdo->beginTransaction();
                $signalStatement = $pdo->prepare(
                    'SELECT device_id FROM ir_signals WHERE id = :id FOR UPDATE'
                );
                $signalStatement->execute([':id' => $signalId]);
                $signal = $signalStatement->fetch();
                if (!$signal) {
                    throw new RuntimeException('Signal not found.');
                }

                $commandStatement = $pdo->prepare(
                    'INSERT INTO ir_commands '
                    . '(device_id, signal_id, status, requested_at) '
                    . "VALUES (:device_id, :signal_id, 'pending', UTC_TIMESTAMP())"
                );
                $commandStatement->execute([
                    ':device_id' => $signal['device_id'],
                    ':signal_id' => $signalId,
                ]);
                $pdo->commit();
                $_SESSION['flash'] = [
                    'type' => 'success',
                    'message' => 'M5へ赤外線送信を依頼しました。',
                ];
            } catch (Throwable $exception) {
                if ($pdo->inTransaction()) {
                    $pdo->rollBack();
                }
                error_log('IR command enqueue error: ' . $exception->getMessage());
                $_SESSION['flash'] = [
                    'type' => 'error',
                    'message' => '送信要求を登録できませんでした。',
                ];
            }
        }

        header('Location: ./', true, 303);
        exit;
    }
}

function h(string $value): string
{
    return htmlspecialchars($value, ENT_QUOTES | ENT_SUBSTITUTE, 'UTF-8');
}

function displayTime(string $utc): string
{
    $date = new DateTimeImmutable($utc, new DateTimeZone('UTC'));
    return $date->setTimezone(new DateTimeZone((string) appConfig()['timezone']))->format('Y/m/d H:i:s');
}

$authenticated = ($_SESSION['authenticated'] ?? false) === true;
$signals = [];
$commands = [];
$flash = $_SESSION['flash'] ?? null;
unset($_SESSION['flash']);
if ($authenticated) {
    if (!isset($_SESSION['csrf_token'])) {
        $_SESSION['csrf_token'] = bin2hex(random_bytes(32));
    }

    $signals = db()->query(
        'SELECT id, device_id, protocol, signal_value, address_value, command_value, bits, carrier_khz, raw_data, raw_length, received_at '
        . 'FROM ir_signals ORDER BY received_at DESC, id DESC LIMIT 100'
    )->fetchAll();
    $commands = db()->query(
        'SELECT c.id, c.device_id, c.signal_id, c.status, c.requested_at, '
        . 'c.claimed_at, c.completed_at, c.error_message, s.protocol '
        . 'FROM ir_commands c INNER JOIN ir_signals s ON s.id = c.signal_id '
        . 'ORDER BY c.requested_at DESC, c.id DESC LIMIT 20'
    )->fetchAll();
}
?>
<!doctype html>
<html lang="ja">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <meta name="robots" content="noindex,nofollow">
  <title>赤外線信号ログ</title>
  <link rel="stylesheet" href="assets/style.css">
</head>
<body>
  <main class="container">
    <header>
      <div><p class="eyebrow">M5StickC Plus</p><h1>赤外線信号ログ</h1></div>
      <?php if ($authenticated): ?>
        <form method="post">
          <input type="hidden" name="action" value="logout">
          <button class="secondary" type="submit">ログアウト</button>
        </form>
      <?php endif; ?>
    </header>

    <?php if (!$authenticated): ?>
      <section class="login card">
        <h2>ログイン</h2>
        <p>閲覧用パスワードを入力してください。</p>
        <?php if ($loginError): ?><p class="error">パスワードが違います。</p><?php endif; ?>
        <form method="post">
          <input type="hidden" name="action" value="login">
          <label for="password">パスワード</label>
          <input id="password" name="password" type="password" required autocomplete="current-password">
          <button type="submit">表示する</button>
        </form>
      </section>
    <?php else: ?>
      <?php if (is_array($flash)): ?>
        <p class="notice <?= h((string) $flash['type']) ?>"><?= h((string) $flash['message']) ?></p>
      <?php endif; ?>

      <section class="command-panel">
        <div class="section-heading">
          <div><p class="eyebrow">REMOTE CONTROL</p><h2>送信履歴</h2></div>
          <span class="live-note">M5は約1秒ごとに確認</span>
        </div>
        <?php if (!$commands): ?>
          <div class="card empty">遠隔操作の履歴はまだありません。</div>
        <?php else: ?>
          <div class="command-list">
            <?php foreach ($commands as $command): ?>
              <article class="command-row">
                <div>
                  <strong>#<?= h((string) $command['id']) ?> <?= h($command['protocol']) ?></strong>
                  <span><?= h($command['device_id']) ?>・信号 #<?= h((string) $command['signal_id']) ?></span>
                </div>
                <div class="command-status">
                  <span class="status <?= h($command['status']) ?>"><?= h($command['status']) ?></span>
                  <time><?= h(displayTime($command['requested_at'])) ?></time>
                </div>
              </article>
            <?php endforeach; ?>
          </div>
        <?php endif; ?>
      </section>

      <p class="summary">最新 <?= count($signals) ?> 件を表示</p>
      <section class="signal-list">
        <?php if (!$signals): ?><div class="card empty">受信データはまだありません。</div><?php endif; ?>
        <?php foreach ($signals as $signal): ?>
          <article class="card signal">
            <div class="signal-title">
              <div><span class="protocol"><?= h($signal['protocol']) ?></span><h2><?= h($signal['device_id']) ?></h2></div>
              <time><?= h(displayTime($signal['received_at'])) ?></time>
            </div>
            <dl>
              <div><dt>値</dt><dd><?= h($signal['signal_value']) ?></dd></div>
              <div><dt>Address</dt><dd><?= h((string) $signal['address_value']) ?></dd></div>
              <div><dt>Command</dt><dd><?= h((string) $signal['command_value']) ?></dd></div>
              <div><dt>Bits</dt><dd><?= h((string) $signal['bits']) ?></dd></div>
              <div><dt>Carrier</dt><dd><?= h((string) $signal['carrier_khz']) ?> kHz</dd></div>
              <div><dt>RAW長</dt><dd><?= h((string) $signal['raw_length']) ?></dd></div>
            </dl>
            <form class="send-form" method="post">
              <input type="hidden" name="action" value="enqueue">
              <input type="hidden" name="csrf_token" value="<?= h($_SESSION['csrf_token']) ?>">
              <input type="hidden" name="signal_id" value="<?= h((string) $signal['id']) ?>">
              <button type="submit">この信号をM5から送信</button>
            </form>
            <details><summary>RAWデータ</summary><pre><?= h($signal['raw_data']) ?></pre></details>
          </article>
        <?php endforeach; ?>
      </section>
    <?php endif; ?>
  </main>
</body>
</html>
