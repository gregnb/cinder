#include "TerminalPage.h"
#include "Constants.h"
#include "TerminalCommandCatalog.h"
#include "TerminalCompletion.h"
#include "TerminalHandlerCommon.h"
#include "crypto/Signer.h"

// Qt
#include <QAbstractTextDocumentLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QScrollBar>
#include <QShowEvent>
#include <QTextDocument>
#include <QTextEdit>
#include <QTextFrame>
#include <QTimer>
#include <QVBoxLayout>

// Services
#include "services/IdlRegistry.h"
#include "services/NetworkStatsService.h"
#include "services/PriceService.h"
#include "services/SolanaApi.h"

// Crypto
#include "crypto/Keypair.h"

// ── Style constants ─────────────────────────────────────────────

static const QString monoFont = "'JetBrains Mono', 'SF Mono', 'Menlo', 'Consolas'";

static const QString termBg = "#0c0e14";
static const QColor promptColor(0x14, 0xF1, 0x95);
static const QColor errorColor(0xef, 0x44, 0x44);
static const QColor dimColor(120, 130, 160);
static const QColor flameColor(0xF9, 0x73, 0x16); // warm orange
static const QColor emberColor(0xFB, 0xBF, 0x24); // bright yellow-orange

// ── Constructor ─────────────────────────────────────────────────

TerminalPage::TerminalPage(SolanaApi* api, IdlRegistry* idlRegistry,
                           NetworkStatsService* networkStats, PriceService* priceService,
                           QWidget* parent)
    : QWidget(parent),
      m_handler(api, idlRegistry, networkStats, priceService, nullptr, nullptr, this) {
    QVBoxLayout* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    // ── Output text area (read-only, supports multi-line selection) ──
    m_output = new QTextEdit();
    m_output->setObjectName("terminalOutput");
    m_output->setReadOnly(true);
    m_output->setFrameShape(QFrame::NoFrame);
    m_output->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    // Cap document size to prevent unbounded memory growth
    m_output->document()->setMaximumBlockCount(15000);

    // Allow mouse selection but keep keyboard focus on input
    m_output->setFocusPolicy(Qt::NoFocus);

    outer->addWidget(m_output, 1);

    // ── Input bar ───────────────────────────────────────────────
    QWidget* inputBar = new QWidget();
    inputBar->setObjectName("terminalInputBar");
    inputBar->setFixedHeight(44);

    QHBoxLayout* inputLayout = new QHBoxLayout(inputBar);
    inputLayout->setContentsMargins(20, 0, 20, 0);
    inputLayout->setSpacing(8);

    QLabel* prompt = new QLabel("$");
    prompt->setProperty("uiClass", "terminalPrompt");
    inputLayout->addWidget(prompt);

    m_input = new QLineEdit();
    m_input->setObjectName("terminalInput");
    m_input->setPlaceholderText(tr("Type a command..."));
    m_input->installEventFilter(this);
    m_output->installEventFilter(this);

    connect(m_input, &QLineEdit::returnPressed, this, [this]() {
        QString cmd = m_input->text().trimmed();
        if (cmd.isEmpty()) {
            return;
        }
        if (!isSensitiveCommand(cmd) && !m_secretFlow.isActive()) {
            m_history.append(cmd);
            if (m_history.size() > 1000) {
                m_history.removeFirst();
            }
            m_historyIndex = m_history.size();
        }
        m_input->clear();
        executeCommand(cmd);
    });

    inputLayout->addWidget(m_input, 1);
    outer->addWidget(inputBar);

    // ── Welcome banner — Braille flame + CINDER ──────────────────
    // Larger font for splash art
    auto banner = [this](const QString& text, const QColor& color) {
        QString escaped = text.toHtmlEscaped().replace(' ', "&nbsp;");
        QString html =
            QString("<span style='color:%1; font-size:16px;'>%2</span>").arg(color.name(), escaped);
        m_output->append(html);
    };

    banner("", dimColor);
    // Flame — centered over CINDER text (center col ~23)
    banner(QStringLiteral(u"                        \u28B1\u28C6"), emberColor);
    banner(QStringLiteral(u"                       \u2808\u28FF\u28F7\u2840"), emberColor);
    banner(QStringLiteral(u"                      \u28B8\u28FF\u28FF\u28F7\u28E7"), flameColor);
    banner(QStringLiteral(u"                    \u28A0\u28FF\u285F\u28FF\u28FF\u28FF\u2847"),
           flameColor);
    banner(QStringLiteral(
               u"                   \u28F3\u28FC\u28FF\u280F\u28B8\u28FF\u28FF\u28FF\u2880"),
           flameColor);
    banner(QStringLiteral(u"                  "
                          u"\u28F0\u28FF\u28FF\u287F\u2801\u28B8\u28FF\u28FF\u285F\u28FC\u2846"),
           flameColor);
    banner(QStringLiteral(u"                \u2880\u28FE\u28FF\u28FF\u281F  "
                          u"\u28FE\u28BF\u28FF\u28FF\u28FF\u28FF"),
           flameColor);
    banner(QStringLiteral(
               u"                \u28FF\u28FF\u28FF\u280F    \u2838\u28FF\u28FF\u28FF\u287F"),
           flameColor);
    banner(
        QStringLiteral(u"                \u28FF\u28FF\u28FF\u2800      \u28B9\u28FF\u287F\u2841"),
        flameColor);
    banner(QStringLiteral(u"                \u2839\u28FF\u28FF\u2844      \u28A0\u28FF\u285E"),
           flameColor);
    banner(QStringLiteral(u"                 \u2808\u281B\u28BF\u28C4    \u28E0\u281E\u280B"),
           flameColor);
    banner(QStringLiteral(u"                     \u2809"), flameColor);
    // CINDER — Standard figlet font (5 lines, taller)
    banner("      ____ ___ _   _ ____  _____ ____", promptColor);
    banner("     / ___|_ _| \\ | |  _ \\| ____|  _ \\", promptColor);
    banner("    | |    | ||  \\| | | | |  _| | |_) |", promptColor);
    banner("    | |___ | || |\\  | |_| | |___|  _ <", promptColor);
    banner("     \\____|___|_| \\_|____/|_____|_| \\_\\", promptColor);
    banner("", dimColor);
    banner("      " + tr("Cinder v%1").arg(AppVersion::string), dimColor);
    banner("      " + tr("Type 'help' for available commands."), dimColor);
    banner("", dimColor);
    scrollToBottom();

    m_handler.setOutputSink(
        [this](const QString& text, const QColor& color) { appendOutput(text, color); });
    m_handler.setSecretSink([this](const QString& label, const QString& secret) {
        m_secretFlow.stageSecret(label, secret, [this](const QString& text, const QColor& color) {
            appendOutput(text, color);
        });
    });

    m_input->setFocus();
}

void TerminalPage::setWalletAddress(const QString& address) {
    m_walletAddress = address;
    m_handler.setWalletAddress(address);
}

void TerminalPage::setKeypair(const Keypair& kp) {
    m_keypair = kp;
    m_handler.setKeypair(kp);
}
void TerminalPage::setSigner(Signer* signer) {
    m_signer = signer;
    m_handler.setSigner(signer);
}

bool TerminalPage::isSensitiveCommand(const QString& cmd) const {
    return m_secretFlow.isSensitiveCommand(cmd);
}

// ═══════════════════════════════════════════════════════════════
//  COMMAND DISPATCH
// ═══════════════════════════════════════════════════════════════

void TerminalPage::executeCommand(const QString& cmd) {
    if (isSensitiveCommand(cmd)) {
        appendPromptLine("[sensitive input hidden]");
    } else {
        appendPromptLine(cmd);
    }

    if (m_secretFlow.handleInput(
            cmd, [this](const QString& text, const QColor& color) { appendOutput(text, color); })) {
        return;
    }

    if (m_handler.consumePendingConfirm(cmd)) {
        return;
    }

    const TerminalParsedCommand parsed = parseTerminalCommand(cmd);
    const TerminalCommandValidation validation = terminalValidateCommand(parsed);
    const QStringList& parts = parsed.parts;
    if (parts.isEmpty()) {
        return;
    }
    if (validation.validity == TerminalCommandValidity::UnknownRoot) {
        appendOutput(validation.message, errorColor);
        appendOutput(tr("Type 'help' for available commands."), dimColor);
        return;
    }
    if (validation.validity == TerminalCommandValidity::UnknownSubcommand) {
        appendOutput(validation.message, errorColor);
        const QString usage = terminalPrimaryUsage(parsed.root);
        if (!usage.isEmpty()) {
            appendOutput(usage, dimColor);
        }
        return;
    }

    switch (parsed.root) {
        case TerminalCommandRoot::Help:
            m_handler.cmdHelp(parts);
            break;
        case TerminalCommandRoot::Clear:
            clearOutput();
            break;
        case TerminalCommandRoot::Address:
            m_handler.cmdAddress();
            break;
        case TerminalCommandRoot::Balance:
            m_handler.cmdBalance();
            break;
        case TerminalCommandRoot::Balances:
            m_handler.cmdBalances();
            break;
        case TerminalCommandRoot::Send:
            m_handler.cmdSend(parts);
            break;
        case TerminalCommandRoot::History:
            m_handler.cmdHistory(parts);
            break;
        case TerminalCommandRoot::Version:
            m_handler.cmdVersion();
            break;
        case TerminalCommandRoot::Echo:
            appendOutput(parts.size() > 1 ? cmd.mid(cmd.indexOf(' ') + 1) : "");
            break;
        case TerminalCommandRoot::Wallet:
            m_handler.cmdWallet(parsed);
            break;
        case TerminalCommandRoot::Key:
            m_handler.cmdKey(parsed);
            break;
        case TerminalCommandRoot::Mnemonic:
            m_handler.cmdMnemonic(parsed);
            break;
        case TerminalCommandRoot::Tx:
            m_handler.cmdTx(parsed);
            break;
        case TerminalCommandRoot::Token:
            m_handler.cmdToken(parsed);
            break;
        case TerminalCommandRoot::Account:
            m_handler.cmdAccount(parsed);
            break;
        case TerminalCommandRoot::Rpc:
            m_handler.cmdRpc(parsed);
            break;
        case TerminalCommandRoot::Contact:
            m_handler.cmdContact(parsed);
            break;
        case TerminalCommandRoot::Portfolio:
            m_handler.cmdPortfolio(parsed);
            break;
        case TerminalCommandRoot::Program:
            m_handler.cmdProgram(parsed);
            break;
        case TerminalCommandRoot::Encode:
            m_handler.cmdEncode(parsed);
            break;
        case TerminalCommandRoot::Db:
            m_handler.cmdDb(parsed);
            break;
        case TerminalCommandRoot::Nonce:
            m_handler.cmdNonce(parsed);
            break;
        case TerminalCommandRoot::Network:
            m_handler.cmdNetwork(parts);
            break;
        case TerminalCommandRoot::Stake:
            m_handler.cmdStake(parsed);
            break;
        case TerminalCommandRoot::Validator:
            m_handler.cmdValidator(parsed);
            break;
        case TerminalCommandRoot::Swap:
            m_handler.cmdSwap(parsed);
            break;
        case TerminalCommandRoot::Price:
            m_handler.cmdPrice(parts);
            break;
        case TerminalCommandRoot::Unknown:
            appendOutput(tr("Command not found: %1").arg(parts[0].toLower()), errorColor);
            break;
    }
}

// ═══════════════════════════════════════════════════════════════
//  OUTPUT HELPERS
// ═══════════════════════════════════════════════════════════════

void TerminalPage::appendOutput(const QString& text, const QColor& color) {
    // Convert spaces to &nbsp; to preserve alignment in monospace output
    QString escaped = text.toHtmlEscaped().replace(' ', "&nbsp;");
    QString html = QString("<span style='color:%1;'>%2</span>").arg(color.name(), escaped);
    m_output->append(html);
    scrollToBottom();
}

void TerminalPage::appendPromptLine(const QString& cmd) {
    QString escaped = cmd.toHtmlEscaped().replace(' ', "&nbsp;");
    QString html = QString("<span style='color:%1;font-weight:700;'>$</span>&nbsp;"
                           "<span style='color:white;'>%2</span>")
                       .arg(promptColor.name(), escaped);
    m_output->append(html);
    scrollToBottom();
}

void TerminalPage::clearOutput() {
    m_output->clear();
    updateOutputMargin();
}

void TerminalPage::scrollToBottom() {
    updateOutputMargin();
    QScrollBar* sb = m_output->verticalScrollBar();
    sb->setValue(sb->maximum());
}

void TerminalPage::updateOutputMargin() {
    QTextDocument* doc = m_output->document();
    QTextFrameFormat fmt = doc->rootFrame()->frameFormat();

    // Content height WITHOUT our dynamic top margin
    qreal currentTop = fmt.topMargin();
    qreal docH = doc->documentLayout()->documentSize().height();
    qreal contentH = docH - currentTop;

    // Push content to bottom of viewport
    qreal viewH = m_output->viewport()->height();
    qreal newTop = qMax(0.0, viewH - contentH);

    if (qAbs(newTop - currentTop) > 0.5) {
        fmt.setTopMargin(newTop);
        doc->rootFrame()->setFrameFormat(fmt);
    }
}

// ═══════════════════════════════════════════════════════════════
//  TAB COMPLETION
// ═══════════════════════════════════════════════════════════════

void TerminalPage::handleTab() {
    const TerminalCompletionResult completion = completeTerminalInput(m_input->text());
    if (!completion.handled) {
        return;
    }

    m_input->setText(completion.updatedInput);

    if (!completion.candidatesToShow.isEmpty()) {
        QString line;
        for (const QString& c : completion.candidatesToShow) {
            line += terminal::padRight(c, 16);
        }
        appendOutput(line, dimColor);
    }
}

// ═══════════════════════════════════════════════════════════════
//  SHOW EVENT — recalc margin when page becomes visible
// ═══════════════════════════════════════════════════════════════

void TerminalPage::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    // Defer so the viewport has its final geometry after the show
    QTimer::singleShot(0, this, [this]() { updateOutputMargin(); });
}

// ═══════════════════════════════════════════════════════════════
//  EVENT FILTER — Command history + Ctrl+C
// ═══════════════════════════════════════════════════════════════

bool TerminalPage::eventFilter(QObject* obj, QEvent* event) {
    // Recalculate top margin when output area is resized
    if (obj == m_output && event->type() == QEvent::Resize) {
        updateOutputMargin();
    }

    if (obj == m_input && event->type() == QEvent::KeyPress) {
        auto* ke = static_cast<QKeyEvent*>(event);
        const bool ctrlOrCmdC =
            ke->key() == Qt::Key_C &&
            ((ke->modifiers() & Qt::ControlModifier) || (ke->modifiers() & Qt::MetaModifier));

        // Secret reveal mode exit keys
        if ((ke->key() == Qt::Key_Escape || ctrlOrCmdC) &&
            m_secretFlow.handleCancelShortcut(
                [this](const QString& text, const QColor& color) { appendOutput(text, color); })) {
            return true;
        }

        // Ctrl+C — cancel pending RPC or clear input
        if (ctrlOrCmdC) {
            if (m_handler.hasPending()) {
                m_handler.cancelPending("^C");
            } else if (m_handler.hasPendingConfirm()) {
                m_handler.cancelPendingConfirm();
                appendOutput("^C", dimColor);
            } else {
                m_input->clear();
                appendOutput("^C", dimColor);
            }
            return true;
        }

        // Tab — autocomplete
        if (ke->key() == Qt::Key_Tab) {
            handleTab();
            return true;
        }

        if (ke->key() == Qt::Key_Up) {
            if (!m_history.isEmpty() && m_historyIndex > 0) {
                m_historyIndex--;
                m_input->setText(m_history[m_historyIndex]);
            }
            return true;
        }
        if (ke->key() == Qt::Key_Down) {
            if (m_historyIndex < m_history.size() - 1) {
                m_historyIndex++;
                m_input->setText(m_history[m_historyIndex]);
            } else {
                m_historyIndex = m_history.size();
                m_input->clear();
            }
            return true;
        }
    }
    return QWidget::eventFilter(obj, event);
}
