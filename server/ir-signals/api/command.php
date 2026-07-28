<?php
declare(strict_types=1);

require dirname(__DIR__) . '/common.php';

if ($_SERVER['REQUEST_METHOD'] !== 'GET') {
    header('Allow: GET');
    jsonResponse(['ok' => false, 'error' => 'method_not_allowed'], 405);
}

requireApiKey();

$deviceId = trim((string) ($_GET['device_id'] ?? ''));
if ($deviceId === '' || strlen($deviceId) > 64) {
    jsonResponse(['ok' => false, 'error' => 'invalid_device_id'], 422);
}

$pdo = db();

try {
    $pdo->beginTransaction();

    // A command whose reply was lost becomes available again after 30 seconds.
    // Normally the M5 retries the completion report without retransmitting.
    $statement = $pdo->prepare(
        "SELECT c.id, c.signal_id, s.carrier_khz, s.raw_data "
        . "FROM ir_commands c "
        . "INNER JOIN ir_signals s ON s.id = c.signal_id "
        . "WHERE c.device_id = :device_id "
        . "AND (c.status = 'pending' "
        . "OR (c.status = 'processing' AND c.claimed_at < UTC_TIMESTAMP() - INTERVAL 30 SECOND)) "
        . "ORDER BY c.requested_at ASC, c.id ASC LIMIT 1 FOR UPDATE"
    );
    $statement->execute([':device_id' => $deviceId]);
    $command = $statement->fetch();

    if (!$command) {
        $pdo->commit();
        http_response_code(204);
        header('Cache-Control: no-store');
        exit;
    }

    $claim = $pdo->prepare(
        "UPDATE ir_commands SET status = 'processing', claimed_at = UTC_TIMESTAMP(), "
        . "error_message = NULL WHERE id = :id"
    );
    $claim->execute([':id' => $command['id']]);
    $pdo->commit();

    $rawData = json_decode((string) $command['raw_data'], true);
    if (!is_array($rawData) || count($rawData) < 1) {
        throw new RuntimeException('Stored RAW data is invalid.');
    }

    jsonResponse([
        'ok' => true,
        'command' => [
            'id' => (int) $command['id'],
            'action' => 'send_ir',
            'signal_id' => (int) $command['signal_id'],
            'carrier_khz' => (int) $command['carrier_khz'],
            'raw_data' => $rawData,
        ],
    ]);
} catch (Throwable $exception) {
    if ($pdo->inTransaction()) {
        $pdo->rollBack();
    }
    error_log('IR command fetch error: ' . $exception->getMessage());
    jsonResponse(['ok' => false, 'error' => 'server_error'], 500);
}
