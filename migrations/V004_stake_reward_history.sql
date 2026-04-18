CREATE TABLE stake_account_rewards (
    stake_address   TEXT    NOT NULL,
    epoch           INTEGER NOT NULL,
    lamports        INTEGER NOT NULL DEFAULT 0,
    post_balance    INTEGER NOT NULL DEFAULT 0,
    effective_slot  INTEGER NOT NULL DEFAULT 0,
    commission      INTEGER,
    updated_at      INTEGER NOT NULL DEFAULT (strftime('%s', 'now')),
    PRIMARY KEY (stake_address, epoch)
);

CREATE INDEX idx_stake_account_rewards_address_epoch
    ON stake_account_rewards(stake_address, epoch DESC);
