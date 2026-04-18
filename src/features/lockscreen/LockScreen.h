#ifndef LOCKSCREEN_H
#define LOCKSCREEN_H

#include "crypto/UnlockResult.h"
#include "features/lockscreen/LockScreenHandler.h"
#include <QWidget>

class QLineEdit;
class QLabel;
class QPushButton;

class LockScreen : public QWidget {
    Q_OBJECT
  public:
    explicit LockScreen(QWidget* parent = nullptr);

    // Call when showing the lock screen to check biometric availability
    void refreshBiometricState();

  protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

  signals:
    void unlocked(const UnlockResult& result);

  private:
    QString unlockErrorMessage(LockScreenHandler::UnlockError error) const;

    QLineEdit* m_passwordInput = nullptr;
    QWidget* m_inputRow = nullptr;
    QLabel* m_errorLabel = nullptr;
    QPushButton* m_unlockBtn = nullptr;

    // Touch ID
    QWidget* m_touchIdBtn = nullptr;
    QLabel* m_touchIdIcon = nullptr;
    QLabel* m_touchIdLabel = nullptr;
    QPixmap m_fpNormal;
    QPixmap m_fpHover;
    QLabel* m_orLabel = nullptr;
    LockScreenHandler* m_handler = nullptr;
};

#endif // LOCKSCREEN_H
