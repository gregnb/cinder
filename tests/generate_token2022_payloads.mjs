// Generate reference payloads for Token-2022 instruction tests.
// Uses @solana/spl-token + @solana/web3.js to create instructions,
// then extracts the serialized message bytes for comparison.
//
// Usage: node tests/generate_token2022_payloads.mjs

import {
  PublicKey,
  SystemProgram,
  TransactionMessage,
  VersionedTransaction,
} from '@solana/web3.js';

import {
  TOKEN_2022_PROGRAM_ID,
  createInitializeMint2Instruction,
  createSetAuthorityInstruction,
  AuthorityType,
  createInitializeMintCloseAuthorityInstruction,
  createInitializeNonTransferableMintInstruction,
  createInitializePermanentDelegateInstruction,
  createInitializeTransferFeeConfigInstruction,
  createTransferCheckedWithFeeInstruction,
  createInitializeDefaultAccountStateInstruction,
  AccountState,
  createEnableRequiredMemoTransfersInstruction,
  createInitializeInterestBearingMintInstruction,
  createEnableCpiGuardInstruction,
  createInitializeTransferHookInstruction,
  createInitializeMetadataPointerInstruction,
  createInitializeGroupPointerInstruction,
  createInitializeGroupMemberPointerInstruction,
  createReallocateInstruction,
  ExtensionType,
} from '@solana/spl-token';

import {
  createInitializeInstruction as createMetadataInitializeInstruction,
  createUpdateFieldInstruction,
  createRemoveKeyInstruction,
  createUpdateAuthorityInstruction as createMetadataUpdateAuthorityInstruction,
} from '@solana/spl-token-metadata';

// Deterministic keys (same as generate_reference_payloads.mjs)
const PAYER        = new PublicKey('BrQgEgNfMCRBJHxSKafDxPqHWRdzmMujSVkbEGiYR3hC');
const RECIPIENT    = new PublicKey('7xKXtg2CW87d97TXJSDpbD5jBkheTqA83TZRuJosgAsU');
const MINT         = new PublicKey('EPjFWdd5AufqSSqeM2qN1xzybapC8G4wEGGkZwyTDt1v');
const SOURCE_ATA   = new PublicKey('DShWnroshVbeUp28oopA3Mc5YhceihJ7rLMv9HXLM36e');
const DEST_ATA     = new PublicKey('4fYNwLBaVso1CJFDnGhNMQWNrveNyGprpjFzF6z3uFQG');
const BLOCKHASH    = new PublicKey('EkSnNWid2cvwEVnVx9aBqawnmiCNiDgp3gUdkDPTKN1N');
const AUTHORITY    = PAYER;
const DELEGATE     = RECIPIENT;
const HOOK_PROGRAM = new PublicKey('Hook111111111111111111111111111111111111111');

function serializeLegacy(payer, blockhash, instructions) {
  const msg = new TransactionMessage({
    payerKey: payer,
    recentBlockhash: blockhash.toBase58(),
    instructions,
  }).compileToLegacyMessage();
  return Buffer.from(msg.serialize()).toString('hex');
}

function printPayload(name, hex) {
  console.log(`// ${name}`);
  console.log(`// ${hex.length / 2} bytes`);
  console.log(`"${hex}"`);
  console.log();
}

// === 1. InitializeMint2 (Token-2022, 6 decimals, with freeze authority) ===
{
  const ix = createInitializeMint2Instruction(
    MINT, 6, PAYER, PAYER, TOKEN_2022_PROGRAM_ID
  );
  printPayload('InitializeMint2_WithFreeze', serializeLegacy(PAYER, BLOCKHASH, [ix]));
}

// === 2. InitializeMint2 (no freeze authority) ===
{
  const ix = createInitializeMint2Instruction(
    MINT, 9, PAYER, null, TOKEN_2022_PROGRAM_ID
  );
  printPayload('InitializeMint2_NoFreeze', serializeLegacy(PAYER, BLOCKHASH, [ix]));
}

// === 3. SetAuthority (CloseMint type, to RECIPIENT) ===
{
  const ix = createSetAuthorityInstruction(
    MINT, PAYER, AuthorityType.CloseMint, RECIPIENT, [], TOKEN_2022_PROGRAM_ID
  );
  printPayload('SetAuthority_CloseMint', serializeLegacy(PAYER, BLOCKHASH, [ix]));
}

// === 4. SetAuthority (revoke — newAuthority = null) ===
{
  const ix = createSetAuthorityInstruction(
    MINT, PAYER, AuthorityType.MintTokens, null, [], TOKEN_2022_PROGRAM_ID
  );
  printPayload('SetAuthority_Revoke', serializeLegacy(PAYER, BLOCKHASH, [ix]));
}

// === 5. InitializeMintCloseAuthority ===
{
  const ix = createInitializeMintCloseAuthorityInstruction(
    MINT, PAYER, TOKEN_2022_PROGRAM_ID
  );
  printPayload('InitializeMintCloseAuthority', serializeLegacy(PAYER, BLOCKHASH, [ix]));
}

// === 6. InitializeNonTransferableMint ===
{
  const ix = createInitializeNonTransferableMintInstruction(
    MINT, TOKEN_2022_PROGRAM_ID
  );
  printPayload('InitializeNonTransferableMint', serializeLegacy(PAYER, BLOCKHASH, [ix]));
}

// === 7. InitializePermanentDelegate ===
{
  const ix = createInitializePermanentDelegateInstruction(
    MINT, DELEGATE, TOKEN_2022_PROGRAM_ID
  );
  printPayload('InitializePermanentDelegate', serializeLegacy(PAYER, BLOCKHASH, [ix]));
}

// === 8. TransferFee::InitializeTransferFeeConfig ===
{
  const ix = createInitializeTransferFeeConfigInstruction(
    MINT, PAYER, PAYER, 100, BigInt(1000000), TOKEN_2022_PROGRAM_ID
  );
  printPayload('TransferFee_InitConfig', serializeLegacy(PAYER, BLOCKHASH, [ix]));
}

// === 9. TransferFee::TransferCheckedWithFee ===
{
  const ix = createTransferCheckedWithFeeInstruction(
    SOURCE_ATA, MINT, DEST_ATA, PAYER, BigInt(1000000), 6, BigInt(100), TOKEN_2022_PROGRAM_ID
  );
  printPayload('TransferFee_TransferCheckedWithFee', serializeLegacy(PAYER, BLOCKHASH, [ix]));
}

// === 10. DefaultAccountState::Initialize (Frozen) ===
{
  const ix = createInitializeDefaultAccountStateInstruction(
    MINT, AccountState.Frozen, TOKEN_2022_PROGRAM_ID
  );
  printPayload('DefaultAccountState_InitFrozen', serializeLegacy(PAYER, BLOCKHASH, [ix]));
}

// === 11. MemoTransfer::Enable ===
{
  const ix = createEnableRequiredMemoTransfersInstruction(
    SOURCE_ATA, PAYER, [], TOKEN_2022_PROGRAM_ID
  );
  printPayload('MemoTransfer_Enable', serializeLegacy(PAYER, BLOCKHASH, [ix]));
}

// === 12. InterestBearingMint::Initialize ===
{
  const ix = createInitializeInterestBearingMintInstruction(
    MINT, PAYER, 500, TOKEN_2022_PROGRAM_ID
  );
  printPayload('InterestBearingMint_Init', serializeLegacy(PAYER, BLOCKHASH, [ix]));
}

// === 13. CpiGuard::Enable ===
{
  const ix = createEnableCpiGuardInstruction(
    SOURCE_ATA, PAYER, [], TOKEN_2022_PROGRAM_ID
  );
  printPayload('CpiGuard_Enable', serializeLegacy(PAYER, BLOCKHASH, [ix]));
}

// === 14. TransferHook::Initialize ===
{
  const ix = createInitializeTransferHookInstruction(
    MINT, PAYER, HOOK_PROGRAM, TOKEN_2022_PROGRAM_ID
  );
  printPayload('TransferHook_Init', serializeLegacy(PAYER, BLOCKHASH, [ix]));
}

// === 15. MetadataPointer::Initialize ===
{
  const ix = createInitializeMetadataPointerInstruction(
    MINT, PAYER, MINT, TOKEN_2022_PROGRAM_ID
  );
  printPayload('MetadataPointer_Init', serializeLegacy(PAYER, BLOCKHASH, [ix]));
}

// === 16. GroupPointer::Initialize ===
{
  const ix = createInitializeGroupPointerInstruction(
    MINT, PAYER, MINT, TOKEN_2022_PROGRAM_ID
  );
  printPayload('GroupPointer_Init', serializeLegacy(PAYER, BLOCKHASH, [ix]));
}

// === 17. GroupMemberPointer::Initialize ===
{
  const ix = createInitializeGroupMemberPointerInstruction(
    MINT, PAYER, MINT, TOKEN_2022_PROGRAM_ID
  );
  printPayload('GroupMemberPointer_Init', serializeLegacy(PAYER, BLOCKHASH, [ix]));
}

// === 18. TokenMetadata::Initialize ===
{
  const ix = createMetadataInitializeInstruction({
    programId: TOKEN_2022_PROGRAM_ID,
    metadata: MINT,
    updateAuthority: PAYER,
    mint: MINT,
    mintAuthority: PAYER,
    name: 'MyToken',
    symbol: 'MTK',
    uri: 'https://example.com',
  });
  printPayload('TokenMetadata_Initialize', serializeLegacy(PAYER, BLOCKHASH, [ix]));
}

// === 19. TokenMetadata::UpdateField (Name) ===
{
  const ix = createUpdateFieldInstruction({
    programId: TOKEN_2022_PROGRAM_ID,
    metadata: MINT,
    updateAuthority: PAYER,
    field: 'Name',
    value: 'NewName',
  });
  // Note: spl-token-metadata Field::Name is NOT the Borsh enum variant 0.
  // The JS SDK encodes it as Key("Name") — variant 3 with string "Name".
  // We need to check what actually comes out.
  printPayload('TokenMetadata_UpdateField_Name', serializeLegacy(PAYER, BLOCKHASH, [ix]));
}

// === 20. TokenMetadata::RemoveKey ===
{
  const ix = createRemoveKeyInstruction({
    programId: TOKEN_2022_PROGRAM_ID,
    metadata: MINT,
    updateAuthority: PAYER,
    key: 'custom_key',
    idempotent: true,
  });
  printPayload('TokenMetadata_RemoveKey', serializeLegacy(PAYER, BLOCKHASH, [ix]));
}

// === 21. Reallocate ===
{
  const ix = createReallocateInstruction(
    SOURCE_ATA,
    PAYER,
    [ExtensionType.MemoTransfer, ExtensionType.CpiGuard],
    PAYER,
    TOKEN_2022_PROGRAM_ID
  );
  printPayload('Reallocate_MemoAndCpiGuard', serializeLegacy(PAYER, BLOCKHASH, [ix]));
}

// === 22. Multi-instruction: Create Token-2022 mint with extensions ===
{
  const ixList = [
    createInitializeMintCloseAuthorityInstruction(MINT, PAYER, TOKEN_2022_PROGRAM_ID),
    createInitializeMetadataPointerInstruction(MINT, PAYER, MINT, TOKEN_2022_PROGRAM_ID),
    createInitializeNonTransferableMintInstruction(MINT, TOKEN_2022_PROGRAM_ID),
    createInitializeMint2Instruction(MINT, 0, PAYER, null, TOKEN_2022_PROGRAM_ID),
    createMetadataInitializeInstruction({
      programId: TOKEN_2022_PROGRAM_ID,
      metadata: MINT,
      updateAuthority: PAYER,
      mint: MINT,
      mintAuthority: PAYER,
      name: 'SoulBound',
      symbol: 'SBT',
      uri: 'https://sbt.example.com/meta.json',
    }),
  ];
  printPayload('MultiIx_CreateMintWithExtensions', serializeLegacy(PAYER, BLOCKHASH, ixList));
}
