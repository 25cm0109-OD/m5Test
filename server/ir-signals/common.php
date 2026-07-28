<?php
declare(strict_types=1);

function appConfig(): array
{
    static $config;
    if ($config === null) {
        $config = require __DIR__ . '/config.php';
    }
    return $config;
}

function db(): PDO
{
    static $pdo;
    if ($pdo === null) {
        $config = appConfig();
        $dsn = sprintf(
            'mysql:host=%s;dbname=%s;charset=utf8mb4',
            $config['db_host'],
            $config['db_name']
        );
        $pdo = new PDO($dsn, $config['db_user'], $config['db_password'], [
            PDO::ATTR_ERRMODE => PDO::ERRMODE_EXCEPTION,
            PDO::ATTR_DEFAULT_FETCH_MODE => PDO::FETCH_ASSOC,
            PDO::ATTR_EMULATE_PREPARES => false,
        ]);
        $pdo->exec("SET time_zone = '+00:00'");
    }
    return $pdo;
}

function jsonResponse(array $body, int $status = 200): void
{
    http_response_code($status);
    header('Content-Type: application/json; charset=utf-8');
    header('Cache-Control: no-store');
    echo json_encode($body, JSON_UNESCAPED_UNICODE | JSON_UNESCAPED_SLASHES);
    exit;
}

function requireApiKey(): void
{
    $providedKey = (string) ($_SERVER['HTTP_X_API_KEY'] ?? '');
    if ($providedKey === '' ||
        !hash_equals((string) appConfig()['api_key'], $providedKey)) {
        jsonResponse(['ok' => false, 'error' => 'unauthorized'], 401);
    }
}
