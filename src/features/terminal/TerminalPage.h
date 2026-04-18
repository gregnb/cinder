#ifndef TERMINALPAGE_H
#define TERMINALPAGE_H

#include <QColor>
#include <QString>
#include <QStringList>
#include <QWidget>

#include "crypto/Keypair.h"
#include "features/terminal/TerminalHandler.h"
#include "features/terminal/TerminalSecretFlow.h"

class QTextEdit;
class QLineEdit;
class QTimer;
class Signer;
class SolanaApi;
class IdlRegistry;
class NetworkStatsService;
class PriceService;

class TerminalPage : public QWidget {
    Q_OBJECT
  public:
    explicit TerminalPage(SolanaApi* api, IdlRegistry* idlRegistry,
                          NetworkStatsService* networkStats, PriceService* priceService,
                          QWidget* parent = nullptr);

    void setWalletAddress(const QString& address);
    void setKeypair(const Keypair& kp);
    void setSigner(Signer* signer);

  protected:
    void showEvent(QShowEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;

  private:
    // UI
    QTextEdit* m_output = nullptr;
    QLineEdit* m_input = nullptr;

    // Dependencies
    QString m_walletAddress;
    Keypair m_keypair;
    Signer* m_signer = nullptr;
    TerminalHandler m_handler;

    // History
    QStringList m_history;
    int m_historyIndex = -1;

    TerminalSecretFlow m_secretFlow;

    // Dispatch
    void executeCommand(const QString& cmd);

    // Output
    void appendOutput(const QString& text, const QColor& color = QColor(255, 255, 255));
    void appendPromptLine(const QString& cmd);
    void clearOutput();
    void scrollToBottom();
    void updateOutputMargin();
    void handleTab();
    bool isSensitiveCommand(const QString& cmd) const;
};

#endif // TERMINALPAGE_H
