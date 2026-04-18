-- Cinder initial consolidated schema

-- ── Wallets ──────────────────────────────────────────────────────

CREATE TABLE wallets (
    id                  INTEGER PRIMARY KEY AUTOINCREMENT,
    label               TEXT    NOT NULL DEFAULT 'My Wallet',
    address             TEXT    NOT NULL UNIQUE,
    key_type            TEXT    NOT NULL CHECK(key_type IN ('mnemonic', 'private_key', 'ledger', 'trezor', 'gridplus')),
    salt                BLOB    NOT NULL,
    nonce               BLOB    NOT NULL,
    ciphertext          BLOB    NOT NULL,
    created_at          INTEGER NOT NULL DEFAULT (strftime('%s', 'now')),
    biometric_enabled   INTEGER NOT NULL DEFAULT 0,
    derivation_path     TEXT,
    hw_plugin           TEXT,
    account_index       INTEGER NOT NULL DEFAULT 0,
    parent_wallet_id    INTEGER REFERENCES wallets(id),
    seed_salt           BLOB,
    seed_nonce          BLOB,
    seed_ciphertext     BLOB,
    mnemonic_salt       BLOB,
    mnemonic_nonce      BLOB,
    mnemonic_ciphertext BLOB,
    avatar_path         TEXT
);

CREATE INDEX idx_wallets_address ON wallets(address);

-- ── Contacts ─────────────────────────────────────────────────────

CREATE TABLE contacts (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    name        TEXT    NOT NULL CHECK(length(name) > 0),
    address     TEXT    NOT NULL UNIQUE CHECK(length(address) > 0),
    created_at  INTEGER NOT NULL DEFAULT (strftime('%s', 'now')),
    avatar_path TEXT
);

CREATE INDEX idx_contacts_name ON contacts(name COLLATE NOCASE);
CREATE INDEX idx_contacts_address ON contacts(address);

-- ── Tokens & Token Accounts ──────────────────────────────────────

CREATE TABLE tokens (
    address          TEXT    PRIMARY KEY,
    symbol           TEXT    NOT NULL CHECK(length(symbol) > 0),
    name             TEXT    NOT NULL CHECK(length(name) > 0),
    decimals         INTEGER NOT NULL DEFAULT 0,
    token_program    TEXT    NOT NULL,
    logo_url         TEXT    DEFAULT '',
    metadata_fetched INTEGER DEFAULT 0,
    coingecko_id     TEXT    DEFAULT NULL,
    mint_authority   TEXT,
    created_at       INTEGER NOT NULL DEFAULT (strftime('%s', 'now')),
    updated_at       INTEGER NOT NULL DEFAULT (strftime('%s', 'now'))
);

CREATE INDEX idx_tokens_coingecko ON tokens(coingecko_id);

CREATE TABLE token_accounts (
    account_address TEXT    NOT NULL PRIMARY KEY,
    token_address   TEXT    NOT NULL REFERENCES tokens(address),
    owner_address   TEXT    NOT NULL,
    balance         TEXT    NOT NULL DEFAULT '0',
    usd_price       REAL    NOT NULL DEFAULT 0.0,
    state           TEXT    NOT NULL DEFAULT 'initialized',
    created_at      INTEGER NOT NULL DEFAULT (strftime('%s', 'now')),
    updated_at      INTEGER NOT NULL DEFAULT (strftime('%s', 'now')),
    UNIQUE (owner_address, token_address)
);

CREATE INDEX idx_token_accounts_owner ON token_accounts(owner_address);
CREATE INDEX idx_token_accounts_token ON token_accounts(token_address);

-- ── Transactions ─────────────────────────────────────────────────

CREATE TABLE transaction_raw (
    signature  TEXT    PRIMARY KEY,
    slot       INTEGER NOT NULL,
    block_time INTEGER,
    raw_json   TEXT    NOT NULL
);

CREATE TABLE transactions (
    id            INTEGER PRIMARY KEY AUTOINCREMENT,
    signature     TEXT    NOT NULL REFERENCES transaction_raw(signature) ON DELETE CASCADE,
    block_time    INTEGER,
    activity_type TEXT    NOT NULL,
    from_address  TEXT,
    to_address    TEXT,
    token         TEXT    NOT NULL,
    amount        REAL    NOT NULL,
    fee           INTEGER,
    slot          INTEGER NOT NULL,
    err           INTEGER NOT NULL DEFAULT 0
);

CREATE INDEX idx_tx_block_time ON transactions(block_time DESC);
CREATE INDEX idx_tx_token      ON transactions(token);
CREATE INDEX idx_tx_type       ON transactions(activity_type);
CREATE INDEX idx_tx_from       ON transactions(from_address);
CREATE INDEX idx_tx_to         ON transactions(to_address);
CREATE INDEX idx_tx_signature  ON transactions(signature);

-- ── Portfolio & Cost Basis ───────────────────────────────────────

CREATE TABLE portfolio_snapshot (
    id            INTEGER PRIMARY KEY AUTOINCREMENT,
    owner_address TEXT    NOT NULL,
    timestamp     INTEGER NOT NULL,
    total_usd     REAL    NOT NULL DEFAULT 0.0,
    sol_price     REAL    NOT NULL DEFAULT 0.0,
    created_at    INTEGER NOT NULL DEFAULT (strftime('%s', 'now'))
);

CREATE INDEX idx_portfolio_snapshot_ts    ON portfolio_snapshot(timestamp);
CREATE INDEX idx_portfolio_snapshot_owner ON portfolio_snapshot(owner_address, timestamp);

CREATE TABLE token_snapshot (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    snapshot_id INTEGER NOT NULL REFERENCES portfolio_snapshot(id) ON DELETE CASCADE,
    mint        TEXT    NOT NULL,
    symbol      TEXT    NOT NULL,
    balance     REAL    NOT NULL DEFAULT 0.0,
    price_usd   REAL    NOT NULL DEFAULT 0.0,
    value_usd   REAL    NOT NULL DEFAULT 0.0
);

CREATE INDEX idx_token_snapshot_sid ON token_snapshot(snapshot_id);

CREATE TABLE cost_basis_lot (
    id            INTEGER PRIMARY KEY AUTOINCREMENT,
    owner_address TEXT    NOT NULL,
    mint          TEXT    NOT NULL,
    symbol        TEXT    NOT NULL,
    acquired_at   INTEGER NOT NULL,
    quantity      REAL    NOT NULL,
    cost_per_unit REAL    NOT NULL DEFAULT 0.0,
    cost_total    REAL    NOT NULL DEFAULT 0.0,
    remaining_qty REAL    NOT NULL,
    source        TEXT    NOT NULL DEFAULT 'unknown',
    tx_signature  TEXT,
    created_at    INTEGER NOT NULL DEFAULT (strftime('%s', 'now'))
);

CREATE INDEX idx_cost_basis_mint       ON cost_basis_lot(mint);
CREATE INDEX idx_cost_basis_remaining  ON cost_basis_lot(mint, remaining_qty);
CREATE INDEX idx_cost_basis_owner_mint ON cost_basis_lot(owner_address, mint);

-- ── Nonce Accounts ───────────────────────────────────────────────

CREATE TABLE nonce_accounts (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    address     TEXT    NOT NULL UNIQUE,
    authority   TEXT    NOT NULL,
    nonce_value TEXT    NOT NULL,
    created_at  INTEGER NOT NULL DEFAULT (strftime('%s', 'now'))
);

CREATE INDEX idx_nonce_authority ON nonce_accounts(authority);

-- ── Anchor IDL Cache ─────────────────────────────────────────────

CREATE TABLE anchor_idl_cache (
    program_id   TEXT    PRIMARY KEY,
    program_name TEXT    NOT NULL,
    idl_json     TEXT    NOT NULL,
    fetched_at   INTEGER NOT NULL DEFAULT (strftime('%s', 'now'))
);

-- ── Validator Cache ──────────────────────────────────────────────

CREATE TABLE validator_cache (
    vote_account TEXT    PRIMARY KEY,
    name         TEXT,
    avatar_url   TEXT,
    score        INTEGER DEFAULT 0,
    version      TEXT    DEFAULT '',
    city         TEXT    DEFAULT '',
    country      TEXT    DEFAULT '',
    updated_at   INTEGER NOT NULL DEFAULT (strftime('%s', 'now'))
);

-- ── Network Stats Cache ──────────────────────────────────────────

CREATE TABLE network_stats_cache (
    id                 INTEGER PRIMARY KEY CHECK (id = 1),
    epoch              INTEGER NOT NULL DEFAULT 0,
    slot_index         INTEGER NOT NULL DEFAULT 0,
    slots_in_epoch     INTEGER NOT NULL DEFAULT 0,
    epoch_progress_pct REAL    NOT NULL DEFAULT 0,
    current_tps        REAL    NOT NULL DEFAULT 0,
    tps_samples        TEXT    NOT NULL DEFAULT '[]',
    total_supply       INTEGER NOT NULL DEFAULT 0,
    circulating_supply INTEGER NOT NULL DEFAULT 0,
    active_stake       INTEGER NOT NULL DEFAULT 0,
    delinquent_pct     REAL    NOT NULL DEFAULT 0,
    validator_count    INTEGER NOT NULL DEFAULT 0,
    absolute_slot      INTEGER NOT NULL DEFAULT 0,
    block_height       INTEGER NOT NULL DEFAULT 0,
    inflation_rate     REAL    NOT NULL DEFAULT 0,
    solana_version     TEXT    NOT NULL DEFAULT '',
    updated_at         INTEGER NOT NULL DEFAULT 0
);

-- ── Sync State ───────────────────────────────────────────────────

CREATE TABLE sync_state (
    address TEXT NOT NULL,
    key     TEXT NOT NULL,
    value   TEXT,
    PRIMARY KEY (address, key)
);

-- ── Notifications ────────────────────────────────────────────────

CREATE TABLE notifications (
    id         INTEGER PRIMARY KEY AUTOINCREMENT,
    type       TEXT    NOT NULL,
    title      TEXT    NOT NULL,
    body       TEXT    NOT NULL,
    signature  TEXT,
    token      TEXT,
    amount     TEXT,
    from_addr  TEXT,
    is_read    INTEGER NOT NULL DEFAULT 0,
    created_at INTEGER NOT NULL
);

-- ── MCP (Agent Access) ───────────────────────────────────────────

CREATE TABLE mcp_access_policies (
    id         INTEGER PRIMARY KEY AUTOINCREMENT,
    name       TEXT    NOT NULL,
    api_key    TEXT    NOT NULL UNIQUE,
    created_at INTEGER NOT NULL DEFAULT (strftime('%s', 'now')),
    updated_at INTEGER NOT NULL DEFAULT (strftime('%s', 'now'))
);

CREATE TABLE mcp_policy_wallets (
    policy_id      INTEGER NOT NULL REFERENCES mcp_access_policies(id) ON DELETE CASCADE,
    wallet_address TEXT    NOT NULL,
    PRIMARY KEY (policy_id, wallet_address)
);

CREATE TABLE mcp_policy_tool_access (
    policy_id  INTEGER NOT NULL REFERENCES mcp_access_policies(id) ON DELETE CASCADE,
    tool_name  TEXT    NOT NULL,
    access     INTEGER NOT NULL DEFAULT 0,
    updated_at INTEGER NOT NULL DEFAULT (strftime('%s', 'now')),
    PRIMARY KEY (policy_id, tool_name)
);

CREATE TABLE mcp_tool_config (
    tool_name  TEXT    PRIMARY KEY,
    enabled    INTEGER NOT NULL DEFAULT 1,
    updated_at INTEGER NOT NULL DEFAULT (strftime('%s', 'now'))
);

CREATE TABLE mcp_wallet_tool_access (
    wallet_address TEXT    NOT NULL,
    tool_name      TEXT    NOT NULL,
    enabled        INTEGER NOT NULL DEFAULT 0,
    updated_at     INTEGER NOT NULL DEFAULT (strftime('%s', 'now')),
    PRIMARY KEY (wallet_address, tool_name)
);

CREATE TABLE mcp_activity_log (
    id             INTEGER PRIMARY KEY AUTOINCREMENT,
    tool_name      TEXT    NOT NULL,
    arguments      TEXT,
    result         TEXT,
    duration_ms    INTEGER,
    success        INTEGER NOT NULL DEFAULT 1,
    wallet_address TEXT,
    policy_id      INTEGER,
    created_at     INTEGER NOT NULL DEFAULT (strftime('%s', 'now'))
);

CREATE INDEX idx_mcp_activity_created ON mcp_activity_log(created_at DESC);

CREATE TABLE mcp_pending_approval (
    id             TEXT    PRIMARY KEY,
    tool_name      TEXT    NOT NULL,
    arguments      TEXT    NOT NULL,
    description    TEXT,
    status         TEXT    NOT NULL DEFAULT 'pending',
    result         TEXT,
    error_msg      TEXT,
    wallet_address TEXT,
    policy_id      INTEGER,
    executed       INTEGER NOT NULL DEFAULT 0,
    created_at     INTEGER NOT NULL,
    resolved_at    INTEGER
);

CREATE INDEX idx_mcp_approval_status ON mcp_pending_approval(status);
