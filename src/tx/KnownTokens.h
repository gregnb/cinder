#ifndef KNOWNTOKENS_H
#define KNOWNTOKENS_H

#include <QMap>
#include <QString>

// Well-known SPL token mint → symbol + icon path + on-chain logo URL
struct KnownToken {
    QString symbol;
    QString iconPath;
    QString logoUrl;
};

// Wrapped SOL mint (SPL Token representation of SOL)
inline const QString WSOL_MINT = "So11111111111111111111111111111111111111112";

// Resolve a mint address to its known symbol and icon.
// WSOL returns "WSOL" — callers that need "SOL" for native contexts
// (SOL transfers, SOL balance changes) should override the display name.
inline KnownToken resolveKnownToken(const QString& mint) {
    static const QMap<QString, KnownToken> knownTokens = {
        {WSOL_MINT, {"WSOL", ":/icons/tokens/sol.png"}},
        {"EPjFWdd5AufqSSqeM2qN1xzybapC8G4wEGGkZwyTDt1v", {"USDC", ":/icons/tokens/usdc.png"}},
        {"Es9vMFrzaCERmJfrF4H2FYD4KCoNkY11McCe8BenwNYB", {"USDT", ":/icons/tokens/usdt.png"}},
        {"DezXAZ8z7PnrnRJjz3wXBoRgixCa6xjnB7YaB1pPB263", {"BONK", ":/icons/tokens/bonk.png"}},
        {"jtojtomepa8beP8AuQc6eXt5FriJwfFMwQx2v2f9mCL", {"JTO", ":/icons/tokens/jto.png"}},
        {"ATLASXmbPQxBUYbxPsV97usA3fPQYEqzQBUHgiFCUsXx", {"ATLAS", ":/icons/tokens/atlas.png"}},
    };

    if (knownTokens.contains(mint))
        return knownTokens[mint];
    return {};
}

#endif // KNOWNTOKENS_H
