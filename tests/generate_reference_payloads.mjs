/**
 * Generate reference transaction payloads using @solana/web3.js v1.
 * These are used to verify the C++ TransactionBuilder produces
 * byte-identical output to the canonical Solana SDK.
 *
 * Run: node tests/generate_reference_payloads.mjs
 */
import {
    PublicKey,
    SystemProgram,
    Transaction,
    TransactionMessage,
    VersionedTransaction,
    TransactionInstruction,
    ComputeBudgetProgram,
    NONCE_ACCOUNT_LENGTH,
} from "@solana/web3.js";

// Deterministic keys (real base58-decodable pubkeys)
const PAYER     = new PublicKey("BrEi3xFm1bQ3K1yGoCbykP3yzGL42MjcxZ9UjGfqKjBo");
const RECIPIENT = new PublicKey("7v91N7iZ9mNicL8WfG6cgSCKyRXydQjLh6UYBWwm6y1Q");
const BLOCKHASH = "EkSnNWid2cvwEVnVx9aBqawnmiCNiDgp3gUdkDPTKN1N";

// Token-related keys
const MINT          = new PublicKey("EPjFWdd5AufqSSqeM2qN1xzybapC8G4wEGGkZwyTDt1v"); // USDC
const TOKEN_PROGRAM = new PublicKey("TokenkegQfeZyiNwAJbNbGKPFXCWuBvf9Ss623VQ5DA");
const TOKEN_2022    = new PublicKey("TokenzQdBNbLqP5VEhdkAS6EPFLC1PHnBqCXEpPxuEb");
const ATA_PROGRAM   = new PublicKey("ATokenGPvbdGVxr1b2hvZbsiqW5xWH25efTNsLJA8knL");
const SOURCE_ATA    = new PublicKey("3EpVCPUgyjq2MfGeCttyey6bs5zya5wjYZ2BE6yDg6bm");
const DEST_ATA      = new PublicKey("J2FLvHxMsRm4c4W3P8MeRZGHGgVMZDCFeCpjtCFJTagt");

// Nonce keys
const NONCE_PUBKEY  = new PublicKey("GNnEBsGMfJCgPkXtwPbJL8nFRjzPEpJGJNmjJA4vi54u");
const NONCE_AUTH    = new PublicKey("5ZWj7a1f8tWkjBESHKgrLmXshuXxqeY9SYcfbshpAqPG");

function toHex(buf) {
    return Buffer.from(buf).toString("hex");
}

function printTest(name, hex) {
    console.log(`\n=== ${name} ===`);
    console.log(`Hex: ${hex}`);
    console.log(`Size: ${hex.length / 2} bytes`);
}

// ─── 1. Legacy SOL Transfer ──────────────────────────────
{
    const tx = new Transaction();
    tx.recentBlockhash = BLOCKHASH;
    tx.feePayer = PAYER;
    tx.add(SystemProgram.transfer({
        fromPubkey: PAYER,
        toPubkey: RECIPIENT,
        lamports: 1_000_000_000,
    }));
    const msg = tx.compileMessage();
    printTest("1_LEGACY_SOL_TRANSFER", toHex(msg.serialize()));
}

// ─── 2. V0 SOL Transfer ──────────────────────────────────
{
    const instructions = [
        SystemProgram.transfer({
            fromPubkey: PAYER,
            toPubkey: RECIPIENT,
            lamports: 1_000_000_000,
        }),
    ];
    const msgV0 = new TransactionMessage({
        payerKey: PAYER,
        recentBlockhash: BLOCKHASH,
        instructions,
    }).compileToV0Message();
    const serialized = msgV0.serialize();
    printTest("2_V0_SOL_TRANSFER", toHex(serialized));
}

// ─── 3. SOL Transfer with Priority Fees ──────────────────
{
    const tx = new Transaction();
    tx.recentBlockhash = BLOCKHASH;
    tx.feePayer = PAYER;
    tx.add(ComputeBudgetProgram.setComputeUnitLimit({ units: 200_000 }));
    tx.add(ComputeBudgetProgram.setComputeUnitPrice({ microLamports: 50_000 }));
    tx.add(SystemProgram.transfer({
        fromPubkey: PAYER,
        toPubkey: RECIPIENT,
        lamports: 1_000_000_000,
    }));
    const msg = tx.compileMessage();
    printTest("3_LEGACY_PRIORITY_FEES", toHex(msg.serialize()));
}

// ─── 4. SPL Token TransferChecked (Token Program) ────────
{
    const tx = new Transaction();
    tx.recentBlockhash = BLOCKHASH;
    tx.feePayer = PAYER;

    // transferChecked: disc=12, amount u64 LE, decimals u8
    const data = Buffer.alloc(10);
    data.writeUInt8(12, 0);           // discriminator
    data.writeBigUInt64LE(1_000_000n, 1); // 1 USDC (6 decimals)
    data.writeUInt8(6, 9);            // decimals

    tx.add(new TransactionInstruction({
        programId: TOKEN_PROGRAM,
        keys: [
            { pubkey: SOURCE_ATA, isSigner: false, isWritable: true },
            { pubkey: MINT,       isSigner: false, isWritable: false },
            { pubkey: DEST_ATA,   isSigner: false, isWritable: true },
            { pubkey: PAYER,      isSigner: true,  isWritable: false },
        ],
        data,
    }));
    const msg = tx.compileMessage();
    printTest("4_SPL_TRANSFER_CHECKED", toHex(msg.serialize()));
}

// ─── 5. Token-2022 TransferChecked ───────────────────────
{
    const tx = new Transaction();
    tx.recentBlockhash = BLOCKHASH;
    tx.feePayer = PAYER;

    const data = Buffer.alloc(10);
    data.writeUInt8(12, 0);
    data.writeBigUInt64LE(1_000_000n, 1);
    data.writeUInt8(6, 9);

    tx.add(new TransactionInstruction({
        programId: TOKEN_2022,
        keys: [
            { pubkey: SOURCE_ATA, isSigner: false, isWritable: true },
            { pubkey: MINT,       isSigner: false, isWritable: false },
            { pubkey: DEST_ATA,   isSigner: false, isWritable: true },
            { pubkey: PAYER,      isSigner: true,  isWritable: false },
        ],
        data,
    }));
    const msg = tx.compileMessage();
    printTest("5_TOKEN2022_TRANSFER_CHECKED", toHex(msg.serialize()));
}

// ─── 6. Create Associated Token Account (Idempotent) ─────
{
    const tx = new Transaction();
    tx.recentBlockhash = BLOCKHASH;
    tx.feePayer = PAYER;

    tx.add(new TransactionInstruction({
        programId: ATA_PROGRAM,
        keys: [
            { pubkey: PAYER,          isSigner: true,  isWritable: true },
            { pubkey: DEST_ATA,       isSigner: false, isWritable: true },
            { pubkey: RECIPIENT,      isSigner: false, isWritable: false },
            { pubkey: MINT,           isSigner: false, isWritable: false },
            { pubkey: SystemProgram.programId, isSigner: false, isWritable: false },
            { pubkey: TOKEN_PROGRAM,  isSigner: false, isWritable: false },
        ],
        data: Buffer.from([0x01]),  // createIdempotent
    }));
    const msg = tx.compileMessage();
    printTest("6_CREATE_ATA_IDEMPOTENT", toHex(msg.serialize()));
}

// ─── 7. Nonce-based SOL Transfer ─────────────────────────
{
    const tx = new Transaction();
    tx.recentBlockhash = BLOCKHASH;  // nonce value used as blockhash
    tx.feePayer = PAYER;

    // AdvanceNonceAccount must be first instruction
    // disc=4 (u32 LE), accounts: [nonce(w), recentBlockhashes(r), authority(s)]
    const RECENT_BLOCKHASHES_SYSVAR = new PublicKey("SysvarRecentB1ockHashes11111111111111111111");
    const advanceData = Buffer.alloc(4);
    advanceData.writeUInt32LE(4, 0);

    tx.add(new TransactionInstruction({
        programId: SystemProgram.programId,
        keys: [
            { pubkey: NONCE_PUBKEY, isSigner: false, isWritable: true },
            { pubkey: RECENT_BLOCKHASHES_SYSVAR, isSigner: false, isWritable: false },
            { pubkey: NONCE_AUTH,   isSigner: true,  isWritable: false },
        ],
        data: advanceData,
    }));

    tx.add(SystemProgram.transfer({
        fromPubkey: PAYER,
        toPubkey: RECIPIENT,
        lamports: 500_000_000,
    }));

    const msg = tx.compileMessage();
    printTest("7_NONCE_SOL_TRANSFER", toHex(msg.serialize()));
}

// ─── 8. Multi-instruction: Create ATA + Token-2022 TransferChecked ───
{
    const tx = new Transaction();
    tx.recentBlockhash = BLOCKHASH;
    tx.feePayer = PAYER;

    // Create ATA idempotent (for Token-2022)
    tx.add(new TransactionInstruction({
        programId: ATA_PROGRAM,
        keys: [
            { pubkey: PAYER,          isSigner: true,  isWritable: true },
            { pubkey: DEST_ATA,       isSigner: false, isWritable: true },
            { pubkey: RECIPIENT,      isSigner: false, isWritable: false },
            { pubkey: MINT,           isSigner: false, isWritable: false },
            { pubkey: SystemProgram.programId, isSigner: false, isWritable: false },
            { pubkey: TOKEN_2022,     isSigner: false, isWritable: false },
        ],
        data: Buffer.from([0x01]),
    }));

    // Token-2022 transferChecked
    const data = Buffer.alloc(10);
    data.writeUInt8(12, 0);
    data.writeBigUInt64LE(5_000_000n, 1);
    data.writeUInt8(6, 9);

    tx.add(new TransactionInstruction({
        programId: TOKEN_2022,
        keys: [
            { pubkey: SOURCE_ATA, isSigner: false, isWritable: true },
            { pubkey: MINT,       isSigner: false, isWritable: false },
            { pubkey: DEST_ATA,   isSigner: false, isWritable: true },
            { pubkey: PAYER,      isSigner: true,  isWritable: false },
        ],
        data,
    }));

    const msg = tx.compileMessage();
    printTest("8_ATA_PLUS_TOKEN2022_TRANSFER", toHex(msg.serialize()));
}

console.log("\n✅ All reference payloads generated successfully.");
