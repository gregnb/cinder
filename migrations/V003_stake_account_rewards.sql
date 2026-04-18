ALTER TABLE stake_accounts
ADD COLUMN total_rewards_lamports INTEGER NOT NULL DEFAULT 0;
