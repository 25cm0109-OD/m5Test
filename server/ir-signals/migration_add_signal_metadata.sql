ALTER TABLE ir_signals
    ADD COLUMN signal_name VARCHAR(100) NOT NULL DEFAULT '' AFTER device_id,
    ADD COLUMN manufacturer VARCHAR(100) NOT NULL DEFAULT '' AFTER signal_name;
