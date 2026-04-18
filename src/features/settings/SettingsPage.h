#ifndef SETTINGSPAGE_H
#define SETTINGSPAGE_H

#include <QWidget>

class QCheckBox;
class QListWidget;
class QPushButton;
class SolanaApi;
class TabBar;
class QStackedWidget;
class SettingsHandler;

class SettingsPage : public QWidget {
    Q_OBJECT
  public:
    explicit SettingsPage(SolanaApi* api, QWidget* parent = nullptr);

  public:
    void setWalletAddress(const QString& address);

    // Programmatic control of biometric toggle (called by CinderWalletApp)
    void setBiometricChecked(bool checked);

  signals:
    void languageChanged(const QString& localeCode);
    void biometricToggled(bool enabled);

  private:
    QWidget* buildGeneralTab();
    QWidget* buildExperimentalTab();
    QWidget* buildAboutTab();
    QWidget* buildRpcEndpointSection();
    QWidget* makeSettingsRow(const QString& title, const QString& desc, QWidget* control);
    QWidget* makeSeparator();
    void syncRpcListToHandler();

    SettingsHandler* m_handler = nullptr;
    QString m_walletAddress;
    TabBar* m_tabs = nullptr;
    QStackedWidget* m_tabStack = nullptr;
    QListWidget* m_rpcList = nullptr;
    QPushButton* m_rpcAddBtn = nullptr;
    QPushButton* m_rpcRemoveBtn = nullptr;
    QCheckBox* m_biometricCb = nullptr;
};

#endif // SETTINGSPAGE_H
