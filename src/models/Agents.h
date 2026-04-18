#ifndef AGENTS_MODEL_H
#define AGENTS_MODEL_H

#include <QString>

struct AgentPolicyCard {
    int id = 0;
    QString name;
    QString apiKey;
    qint64 createdAt = 0;
    int walletCount = 0;
    int apiCalls = 0;
    int pending = 0;
    QString permissionSummary;
};

#endif // AGENTS_MODEL_H
