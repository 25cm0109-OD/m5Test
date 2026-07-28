<?php
declare(strict_types=1);

require dirname(__DIR__) . '/common.php';

if ($_SERVER['REQUEST_METHOD'] !== 'POST') {
    header('Allow: POST');
    jsonResponse(['ok' => false, 'error' => 'method_not_allowed'], 405);
}

requireApiKey();

$requestBody = (string) file_get_contents('php://input');
if (strlen($requestBody) > 65536) {
    jsonResponse(['ok' => false, 'error' => 'payload_too_large'], 413);
}

$input = json_decode($requestBody, true);
if (!is_array($input)) {
    jsonResponse(['ok' => false, 'error' => 'invalid_json'], 400);
}

$deviceId = trim((string) ($input['device_id'] ?? ''));
$protocol = trim((string) ($input['protocol'] ?? ''));
$signalValue = trim((string) ($input['value'] ?? ''));
$rawData = $input['raw_data'] ?? null;
$address = filter_var($input['address'] ?? null, FILTER_VALIDATE_INT, ['options' => ['min_range' => 0]]);
$command = filter_var($input['command'] ?? null, FILTER_VALIDATE_INT, ['options' => ['min_range' => 0]]);
$bits = filter_var($input['bits'] ?? null, FILTER_VALIDATE_INT, ['options' => ['min_range' => 0, 'max_range' => 2048]]);
$carrierKhz = filter_var($input['carrier_khz'] ?? null, FILTER_VALIDATE_INT, ['options' => ['min_range' => 20, 'max_range' => 100]]);

if ($deviceId === '' || strlen($deviceId) > 64 ||
    $protocol === '' || strlen($protocol) > 32 ||
    !preg_match('/^0x[0-9A-Fa-f]{1,16}$/', $signalValue) ||
    $address === false || $command === false || $bits === false ||
    $carrierKhz === false || !is_array($rawData) ||
    count($rawData) < 1 || count($rawData) > 2048) {
    jsonResponse(['ok' => false, 'error' => 'invalid_fields'], 422);
}

$validatedRaw = [];
foreach ($rawData as $duration) {
    $duration = filter_var($duration, FILTER_VALIDATE_INT, ['options' => ['min_range' => 1, 'max_range' => 65535]]);
    if ($duration === false) {
        jsonResponse(['ok' => false, 'error' => 'invalid_raw_data'], 422);
    }
    $validatedRaw[] = $duration;
}

try {
    $statement = db()->prepare(
        'INSERT INTO ir_signals '
        . '(device_id, protocol, signal_value, address_value, command_value, bits, carrier_khz, raw_data, raw_length, received_at) '
        . 'VALUES (:device_id, :protocol, :signal_value, :address_value, :command_value, :bits, :carrier_khz, :raw_data, :raw_length, :received_at)'
    );
    $statement->execute([
        ':device_id' => $deviceId,
        ':protocol' => $protocol,
        ':signal_value' => strtoupper($signalValue),
        ':address_value' => $address,
        ':command_value' => $command,
        ':bits' => $bits,
        ':carrier_khz' => $carrierKhz,
        ':raw_data' => json_encode($validatedRaw),
        ':raw_length' => count($validatedRaw),
        ':received_at' => gmdate('Y-m-d H:i:s'),
    ]);
    jsonResponse(['ok' => true, 'id' => (int) db()->lastInsertId()], 201);
} catch (Throwable $exception) {
    error_log('IR upload error: ' . $exception->getMessage());
    jsonResponse(['ok' => false, 'error' => 'server_error'], 500);
}
