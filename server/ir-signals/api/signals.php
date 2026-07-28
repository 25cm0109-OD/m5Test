<?php
declare(strict_types=1);

require dirname(__DIR__) . '/common.php';

if ($_SERVER['REQUEST_METHOD'] !== 'GET') {
    header('Allow: GET');
    jsonResponse(['ok' => false, 'error' => 'method_not_allowed'], 405);
}

$config = appConfig();
$providedKey = (string) ($_SERVER['HTTP_X_VIEWER_KEY'] ?? '');
if ($providedKey === '' || !hash_equals((string) $config['viewer_password'], $providedKey)) {
    jsonResponse(['ok' => false, 'error' => 'unauthorized'], 401);
}

$limit = filter_var(
    $_GET['limit'] ?? 100,
    FILTER_VALIDATE_INT,
    ['options' => ['min_range' => 1, 'max_range' => 100]]
);
if ($limit === false) {
    jsonResponse(['ok' => false, 'error' => 'invalid_limit'], 422);
}

try {
    $statement = db()->prepare(
        'SELECT id, device_id, protocol, signal_value, address_value, command_value, '
        . 'bits, carrier_khz, raw_data, raw_length, received_at '
        . 'FROM ir_signals ORDER BY received_at DESC, id DESC LIMIT :limit'
    );
    $statement->bindValue(':limit', $limit, PDO::PARAM_INT);
    $statement->execute();

    $signals = [];
    foreach ($statement->fetchAll() as $row) {
        $row['id'] = (int) $row['id'];
        $row['bits'] = (int) $row['bits'];
        $row['carrier_khz'] = (int) $row['carrier_khz'];
        $row['raw_length'] = (int) $row['raw_length'];
        $row['raw_data'] = json_decode((string) $row['raw_data'], true) ?: [];
        $signals[] = $row;
    }
    jsonResponse(['ok' => true, 'signals' => $signals]);
} catch (Throwable $exception) {
    error_log('IR list error: ' . $exception->getMessage());
    jsonResponse(['ok' => false, 'error' => 'server_error'], 500);
}
