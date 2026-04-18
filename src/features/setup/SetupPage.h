#ifndef SETUPPAGE_H
#define SETUPPAGE_H

#include "crypto/UnlockResult.h"
#include "features/setup/SetupHandler.h"
#include <QColor>
#include <QLabel>
#include <QWidget>

struct HWDeviceInfo;

class AddressLink;
class QStackedWidget;
class QGridLayout;
class QLineEdit;
class QPushButton;

class SetupPage : public QWidget {
    Q_OBJECT
  public:
    explicit SetupPage(QWidget* parent = nullptr);

    void setAvailableLedgerDevices(const QList<HWDeviceInfo>& devices);
    void onLedgerAddressReceived(const QString& address, const QByteArray& pubkey);
    void onLedgerConnectionFailed(const QString& error);

    void setAvailableTrezorDevices(const QList<HWDeviceInfo>& devices);
    void onTrezorAddressReceived(const QString& address, const QByteArray& pubkey);
    void onTrezorConnectionFailed(const QString& error);

    // Skip password step and jump straight to mnemonic creation (used by Add Account)
    void startCreateFlow(const QString& sessionPassword);

    // Skip password step and show import options only (no Create) — used by Add Wallet
    void startImportFlow(const QString& sessionPassword);

  signals:
    void backRequested();
    void walletCreated(const UnlockResult& result);
    void ledgerPageEntered();
    void ledgerConnectionRequested(const QString& deviceId, const QString& derivationPath);
    void trezorPageEntered();
    void trezorConnectionRequested(const QString& deviceId, const QString& derivationPath);
    void latticeConnectionRequested(const QString& deviceId, const QString& derivationPath);
    void latticePairingSubmitted(const QString& pairingCode);

  protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

  private:
    QString m_pendingPassword;

    // Internal page routing (matches addWidget order in constructor)
    enum class Step {
        Welcome = 0,
        SetupCards,
        SetPassword,
        CreateWallet,
        ConfirmRecovery,
        ImportRecovery,
        ImportPrivateKey,
        Success,
        SelectHardwareType,
        LedgerDetect,
        TrezorDetect,
        LatticeDetect
    };
    void showStep(Step step);

    // What the user chose on SetupCards — determines where SetPassword routes next
    Step m_pendingWalletFlow = Step::CreateWallet;

    QStackedWidget* m_stack = nullptr;

    // Welcome page
    QWidget* buildWelcomePage();
    void showTosModal();

    // Set password page
    QLineEdit* m_passwordInput = nullptr;
    QLineEdit* m_confirmPasswordInput = nullptr;
    QLabel* m_passwordError = nullptr;
    QPushButton* m_passwordContinueBtn = nullptr;

    // Create wallet page
    QLabel* m_seedWordLabels[24] = {};
    QString m_generatedMnemonic;
    QLineEdit* m_createPassphraseInput = nullptr;

    // Inline error labels (one per page that needs it)
    QLabel* m_confirmError = nullptr;
    QLabel* m_importError = nullptr;
    QLabel* m_importKeyError = nullptr;
    QLabel* m_createError = nullptr;

    // Confirm recovery page
    QGridLayout* m_confirmGrid = nullptr;
    QLabel* m_confirmWordLabels[24] = {};
    QLineEdit* m_confirmInputs[5] = {};
    int m_challengeIndices[5] = {};
    QPushButton* m_confirmBtn = nullptr;

    // Import wallet page
    QLineEdit* m_importWordInputs[24] = {};
    QLineEdit* m_importPassphraseInput = nullptr;

    // Import private key page
    QLineEdit* m_privateKeyInput = nullptr;

    // Ledger detect page
    QLabel* m_ledgerStatusLabel = nullptr;
    QLabel* m_ledgerDeviceLabel = nullptr;
    QPushButton* m_ledgerConnectBtn = nullptr;
    QLineEdit* m_derivationPathInput = nullptr;
    QString m_ledgerDeviceId;
    QString m_ledgerAddress;
    QByteArray m_ledgerPubkey;

    // Trezor detect page
    QLabel* m_trezorStatusLabel = nullptr;
    QLabel* m_trezorDeviceLabel = nullptr;
    QPushButton* m_trezorConnectBtn = nullptr;
    QLineEdit* m_trezorDerivationPathInput = nullptr;
    QString m_trezorDeviceId;
    QString m_trezorAddress;
    QByteArray m_trezorPubkey;

    // Lattice detect page
    QLabel* m_latticeStatusLabel = nullptr;
    QPushButton* m_latticeConnectBtn = nullptr;
    QLineEdit* m_latticeDeviceIdInput = nullptr;
    QLineEdit* m_latticePairingInput = nullptr;
    QWidget* m_latticePairingContainer = nullptr;

    // Success page
    AddressLink* m_successAddress = nullptr;
    bool m_confettiPending = false;
    Keypair m_createdKeypair;
    WalletKeyType m_completedWalletType = WalletKeyType::Unknown;

    bool m_importMode = false;
    QWidget* m_createCard = nullptr;
    QWidget* m_importRecoveryCard = nullptr;
    QWidget* m_importKeyCard = nullptr;
    QWidget* m_hardwareCard = nullptr;
    QGridLayout* m_cardGrid = nullptr;

    QWidget* buildSetupCards();
    QWidget* buildSetPasswordPage();
    QWidget* buildCreateWallet();
    QWidget* buildConfirmRecovery();
    QWidget* buildImportWallet();
    QWidget* buildImportPrivateKey();
    QWidget* buildSelectHardwareType();
    QWidget* buildLedgerDetect();
    QWidget* buildTrezorDetect();
    QWidget* buildLatticeDetect();
    QWidget* buildSuccessPage();

    void showSuccess(const QString& address);
    void spawnConfetti();
    void generateNewMnemonic();
    void showConfirmPage();
    void validateConfirmation();
    void finalizeCreateWallet();
    void finalizeImportWallet();
    void finalizeImportPrivateKey();
    void finalizeLedgerWallet();
    void finalizeTrezorWallet();
    void handleWalletCompletion(const SetupHandler::WalletCompletion& completion);

    QWidget* createActionCard(const QString& iconPath, const QColor& accent, const QString& title,
                              const QString& subtitle, int iconSize);
    void applyCardStyle(QWidget* card, const QColor& accent, double borderOpacity);

    SetupHandler m_handler;
    SetupHandler::RecoveryChallenge m_recoveryChallenge;
};

#endif // SETUPPAGE_H
