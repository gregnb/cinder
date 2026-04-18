#ifndef ADDRESSINPUT_H
#define ADDRESSINPUT_H

#include <QWidget>

class QLineEdit;
class QListWidget;
class QListWidgetItem;

class AddressInput : public QWidget {
    Q_OBJECT
  public:
    explicit AddressInput(QWidget* parent = nullptr);

    QString address() const;
    void setAddress(const QString& address);

  signals:
    void contactSelected(const QString& name, const QString& address);
    void addressChanged(const QString& text);

  protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

  private:
    QLineEdit* m_input = nullptr;
    QWidget* m_popup = nullptr;
    QListWidget* m_list = nullptr;

    struct Contact {
        QString name;
        QString address;
    };
    QList<Contact> m_contacts;

    void loadContacts();
    void filterContacts(const QString& text);
    void showPopup();
    void hidePopup();
    void onItemClicked(QListWidgetItem* item);
};

#endif // ADDRESSINPUT_H
