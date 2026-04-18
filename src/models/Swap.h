#ifndef SWAP_MODEL_H
#define SWAP_MODEL_H

#include <QList>
#include <QString>

struct SwapTokenOption {
    QString iconPath;
    QString displayName;
    QString balanceText;
    QString mint;
};

struct SwapQuoteView {
    QString estimatedOutput;
    QString rateText;
    QString routeText;
    QString priceImpactText;
    QString minReceivedText;
    QString slippageText;
    bool highPriceImpact = false;
};

#endif // SWAP_MODEL_H
