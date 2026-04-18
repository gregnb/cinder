-- Stake account cache for instant dashboard display

CREATE TABLE stake_accounts (
    address         TEXT    NOT NULL,
    wallet_address  TEXT    NOT NULL,
    lamports        INTEGER NOT NULL DEFAULT 0,
    vote_account    TEXT    NOT NULL DEFAULT '',
    stake           INTEGER NOT NULL DEFAULT 0,
    state           TEXT    NOT NULL DEFAULT 'Unknown',
    activation_epoch    INTEGER NOT NULL DEFAULT 0,
    deactivation_epoch  INTEGER NOT NULL DEFAULT 0,
    updated_at      INTEGER NOT NULL DEFAULT (strftime('%s', 'now')),
    PRIMARY KEY (address, wallet_address)
);

CREATE INDEX idx_stake_accounts_wallet ON stake_accounts(wallet_address);
