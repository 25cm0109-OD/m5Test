CREATE TABLE ir_signals (
    id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    device_id VARCHAR(64) NOT NULL,
    protocol VARCHAR(32) NOT NULL,
    signal_value VARCHAR(18) NOT NULL,
    address_value BIGINT UNSIGNED NOT NULL,
    command_value BIGINT UNSIGNED NOT NULL,
    bits SMALLINT UNSIGNED NOT NULL,
    carrier_khz SMALLINT UNSIGNED NOT NULL,
    raw_data MEDIUMTEXT NOT NULL,
    raw_length SMALLINT UNSIGNED NOT NULL,
    received_at DATETIME NOT NULL,
    PRIMARY KEY (id),
    INDEX idx_received_at (received_at),
    INDEX idx_device_received (device_id, received_at)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

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
