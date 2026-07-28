-- schema.sql をすでにインポート済みの場合だけ、このファイルを1回実行します。
CREATE TABLE ir_commands (
    id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    device_id VARCHAR(64) NOT NULL,
    signal_id BIGINT UNSIGNED NOT NULL,
    status VARCHAR(16) NOT NULL DEFAULT 'pending',
    requested_at DATETIME NOT NULL,
    claimed_at DATETIME NULL,
    completed_at DATETIME NULL,
    error_message VARCHAR(255) NULL,
    PRIMARY KEY (id),
    INDEX idx_device_status_requested (device_id, status, requested_at),
    INDEX idx_signal_id (signal_id),
    CONSTRAINT fk_ir_commands_signal
        FOREIGN KEY (signal_id) REFERENCES ir_signals (id)
        ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
