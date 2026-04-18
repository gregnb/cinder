#!/usr/bin/env node
// Fetches Anchor IDLs from on-chain for bundled programs.
// Usage: node scripts/fetch_idls.mjs

import { Connection, PublicKey } from '@solana/web3.js';
import { createHash } from 'crypto';
import { inflateSync } from 'zlib';
import { writeFileSync, mkdirSync } from 'fs';

const RPC = 'https://api.mainnet-beta.solana.com';
const conn = new Connection(RPC);

const PROGRAMS = [
  { id: 'JUP6LkbZbjS1jKKwapdHNy74zcZ3tLUZoi5QNyVTaV4', file: 'jupiter_v6' },
  { id: '675kPX9MHTjS2zt1qfr1NYHuzeLXfQM9H24wFSUt1Mp8', file: 'raydium_amm_v4' },
  { id: 'CAMMCzo5YL8w4VFF8KVHrK22GGUsp5VTaW7grrKgrWqK', file: 'raydium_clmm' },
  { id: 'CPMMoo8L3F4NbTegBCKVNunggL7H1ZpdTHKxQB5qKP1C', file: 'raydium_cp_swap' },
  { id: 'whirLbMiicVdio4qvUfM5KAg6Ct8VwpYzGff3uctyCc', file: 'orca_whirlpool' },
  { id: 'LBUZKhRxPF3XUpBCjp4YzTKgLccjZhTSDM9YuVaPwxo', file: 'meteora_dlmm' },
  { id: 'PhoeNiXZ8ByJGLkxNfZRnkUfjvmuYqLR89jjFHGqdXY', file: 'phoenix_dex' },
  { id: 'dRiftyHA39MWEi3m9aunc5MzRF1JYuBsbn6VPcn33UH', file: 'drift_v2' },
  { id: '4MangoMjqJ2firMokCjjGPuFqYHLfMY4qiR5M34EcE1', file: 'mango_v4' },
  { id: 'MarBmsSgKXdrN1egZf5sqe1TMai9K1rChYNDJgjq7aD', file: 'marinade' },
  { id: 'TSWAPaqyCSx2KABk68Shruf4rp7CxcNi8hAsbdwmHbN', file: 'tensor_tswap' },
  { id: 'M2mx93ekt1fmXSVkTrUL9xVFHkmME8HTUi5Cyc5aF7K', file: 'magic_eden_m2' },
  { id: 'SQDS4ep65T869zMMBKyuUq6aD6EgTu8psMjkvj52pCf', file: 'squads_v4' },
  { id: '6EF8rrecthR5Dkzon8Nwu78hRvfCKubJ14M5uBEwF6P', file: 'pump_fun' },
];

async function deriveIdlAddress(programId) {
  const pid = new PublicKey(programId);
  const [base] = PublicKey.findProgramAddressSync([], pid);
  return await PublicKey.createWithSeed(base, 'anchor:idl', pid);
}

function computeDiscriminator(name) {
  const hash = createHash('sha256').update(`global:${name}`).digest();
  return Array.from(hash.subarray(0, 8));
}

function stripIdl(idl, programId) {
  const stripped = {
    address: programId,
    metadata: {
      name: idl.metadata?.name || idl.name || '',
      version: idl.metadata?.version || idl.version || '',
      spec: idl.metadata?.spec || '',
    },
    instructions: [],
  };

  for (const ix of (idl.instructions || [])) {
    const strippedIx = {
      name: ix.name,
      discriminator: ix.discriminator || computeDiscriminator(ix.name),
      accounts: (ix.accounts || []).map(acc => ({
        name: acc.name,
        writable: acc.writable ?? acc.isMut ?? false,
        signer: acc.signer ?? acc.isSigner ?? false,
      })),
    };
    stripped.instructions.push(strippedIx);
  }

  return stripped;
}

async function fetchIdl(programId) {
  const idlAddr = await deriveIdlAddress(programId);
  console.log(`  IDL address: ${idlAddr.toBase58()}`);

  const info = await conn.getAccountInfo(idlAddr);
  if (!info || !info.data) {
    return null;
  }

  // Layout: [8-byte disc][32-byte authority][4-byte len][zlib data]
  const data = info.data;
  if (data.length < 44) return null;

  const dataLen = data.readUInt32LE(40);
  const compressed = data.subarray(44, 44 + dataLen);

  const inflated = inflateSync(compressed);
  return JSON.parse(inflated.toString('utf-8'));
}

async function main() {
  mkdirSync('idl', { recursive: true });

  let success = 0;
  let failed = 0;

  for (const prog of PROGRAMS) {
    console.log(`\nFetching ${prog.file} (${prog.id})...`);
    try {
      const idl = await fetchIdl(prog.id);
      if (!idl) {
        console.log(`  ❌ No IDL found on-chain`);
        failed++;
        continue;
      }

      const stripped = stripIdl(idl, prog.id);
      const path = `idl/${prog.file}.json`;
      writeFileSync(path, JSON.stringify(stripped, null, 2) + '\n');
      console.log(`  ✅ ${stripped.instructions.length} instructions → ${path}`);
      success++;

      // Rate limit to avoid 429s
      await new Promise(r => setTimeout(r, 500));
    } catch (err) {
      console.log(`  ❌ Error: ${err.message}`);
      failed++;
    }
  }

  console.log(`\nDone: ${success} fetched, ${failed} failed`);
}

main().catch(console.error);
