#ifndef MACUTILS_H
#define MACUTILS_H

#include <QString>
#include <QWindow>
#include <functional>

void setupTransparentTitleBar(QWindow* window);
void updateNotificationBadge(int count);
void setSidebarToggleCallback(std::function<void()> callback);
void setBellClickCallback(std::function<void()> callback);
void updateConnectionStatus(bool connected, const QString& text);
void updateSidebarToggleTooltip(const QString& tooltip);
void updateNotificationBellTooltip(const QString& tooltip);

void setToolbarItemsVisible(bool visible);
void setPointingHandCursorForPopup();
void restoreDefaultCursorForPopup();

// Biometric (Touch ID) + Keychain
bool isBiometricAvailable();
bool storeBiometricPassword(const QString& walletAddress, const QString& password);
bool retrieveBiometricPassword(const QString& walletAddress, QString& outPassword);
bool hasStoredBiometricPassword(const QString& walletAddress);
bool deleteBiometricPassword(const QString& walletAddress);

// Read Claude Code's OAuth token from macOS Keychain
bool readClaudeCodeOAuthToken(QString& outToken);

// Sleep/Wake power notifications
void registerSleepWakeCallbacks(std::function<void()> onSleep, std::function<void()> onWake);
void unregisterSleepWakeCallbacks();

#endif // MACUTILS_H
