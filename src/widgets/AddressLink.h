#ifndef ADDRESSLINK_H
#define ADDRESSLINK_H

#include <QHash>
#include <QLabel>
#include <QList>
#include <QPushButton>
#include <QWidget>

class AddressLink : public QWidget {
    Q_OBJECT
  public:
    explicit AddressLink(const QString& address, QWidget* parent = nullptr);
    ~AddressLink() override;

    void setAddress(const QString& address);
    QString address() const;
    void setMaxDisplayChars(int maxChars);

    // Show contact name + optional avatar instead of raw address.
    // The raw address is still used for copy, tooltip, and hover registry.
    void setContactInfo(const QString& name, const QString& avatarPath);

  protected:
    bool event(QEvent* event) override;

  private:
    QLabel* m_avatarLabel = nullptr;
    QLabel* m_label = nullptr;
    QLabel* m_tooltip = nullptr;
    QPushButton* m_copyBtn = nullptr;
    QString m_address;
    int m_maxDisplayChars = 0;

    void updateDisplayText();
    void applyDefaultStyle();
    void applyHoverStyle();

    void registerLink();
    void unregisterLink();

    // Global registry: address -> list of all live AddressLink widgets for that address
    static QHash<QString, QList<AddressLink*>> s_registry;
};

#endif // ADDRESSLINK_H
