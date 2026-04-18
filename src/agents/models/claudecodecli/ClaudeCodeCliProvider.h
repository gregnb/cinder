#ifndef CLAUDECODECLIPROVIDER_H
#define CLAUDECODECLIPROVIDER_H

#include "agents/ModelProvider.h"

class ClaudeCodeCliProvider : public ModelProvider {
    Q_OBJECT
  public:
    explicit ClaudeCodeCliProvider(QObject* parent = nullptr);

    QString id() const override;
    QString displayName() const override;
    QString configSnippet() const override;
    QString configFormat() const override;
};

#endif // CLAUDECODECLIPROVIDER_H
