<?php
declare(strict_types=1);

require dirname(__DIR__) . '/common.php';

if ($_SERVER['REQUEST_METHOD'] !== 'POST') {
    header('Allow: POST');
    jsonResponse(['ok' => false, 'error' => 'method_not_allowed'], 405);
}

requireApiKey();

$requestBody = (string) file_get_contents('php://input');
if (strlen($requestBody) > 4096) {
    jsonResponse(['ok' => false, 'error' => 'payload_too_large'], 413);
}

$input = json_decode($requestBody, true);
if (!is_array($input)) {
    jsonResponse(['ok' => false, 'error' => 'invalid_json'], 400);
}

$deviceId = trim((string) ($input['device_id'] ?? ''));
$commandId = filter_var(
    $input['command_id'] ?? null,
    FILTER_VALIDATE_INT,
    ['options' => ['min_range' => 1]]
);
$status = (string) ($input['status'] ?? '');
$errorMessage = trim((string) ($input['error'] ?? ''));

if ($deviceId === '' || strlen($deviceId) > 64 ||
    $commandId === false ||
    !in_array($status, ['completed', 'failed'], true) ||
    strlen($errorMessage) > 255) {
    jsonResponse(['ok' => false, 'error' => 'invalid_fields'], 422);
}

try {
    $statement = db()->prepare(
        'UPDATE ir_commands SET status = :status, completed_at = UTC_TIMESTAMP(), '
        . 'error_message = :error_message '
        . "WHERE id = :id AND device_id = :device_id "
        . "AND status IN ('processing', 'completed', 'failed')"
    );
    $statement->execute([
        ':status' => $status,
        ':error_message' => $errorMessage === '' ? null : $errorMessage,
        ':id' => $commandId,
        ':device_id' => $deviceId,
    ]);

    if ($statement->rowCount() < 1) {
        $check = db()->prepare(
            'SELECT status FROM ir_commands WHERE id = :id AND device_id = :device_id'
        );
        $check->execute([':id' => $commandId, ':device_id' => $deviceId]);
        if (!$check->fetch()) {
            jsonResponse(['ok' => false, 'error' => 'command_not_found'], 404);
        }
    }

    jsonResponse(['ok' => true]);
} catch (Throwable $exception) {
    error_log('IR command result error: ' . $exception->getMessage());
    jsonResponse(['ok' => false, 'error' => 'server_error'], 500);
}
