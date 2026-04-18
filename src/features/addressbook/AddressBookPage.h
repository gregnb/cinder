#ifndef ADDRESSBOOKPAGE_H
#define ADDRESSBOOKPAGE_H

#include "features/addressbook/AddressBookHandler.h"
#include <QWidget>
#include <functional>

class QVBoxLayout;
class QLineEdit;
class QPushButton;
class QStackedWidget;
class QLabel;
class UploadWidget;

class AddressBookPage : public QWidget {
    Q_OBJECT
  public:
    explicit AddressBookPage(QWidget* parent = nullptr);

  public slots:
    void refreshList();

  signals:
    void sendToAddress(const QString& address);
    void contactsChanged();

  private:
    enum class Step { List = 0, ContactForm };
    void showStep(Step step);

    QStackedWidget* m_stack = nullptr;
    QVBoxLayout* m_listLayout = nullptr;
    QLineEdit* m_nameInput = nullptr;
    QLineEdit* m_addressInput = nullptr;
    QLabel* m_addressError = nullptr;
    QLineEdit* m_searchInput = nullptr;
    QPushButton* m_saveBtn = nullptr;
    QLabel* m_formTitle = nullptr;
    UploadWidget* m_avatarUpload = nullptr;
    AddressBookHandler m_handler;

    int m_editContactId = -1; // -1 = add mode, >= 0 = edit mode
    QString m_editOriginalAddress;

    QWidget* buildListView();
    QWidget* buildContactForm();
    QWidget* createContactRow(const AddressBookContactView& contact, bool alternate);
    void openAddForm();
    void openEditForm(int contactId, const QString& name, const QString& address);
    void updateSaveButtonState();
    void confirmDeleteAsync(const QString& contactName,
                            const std::function<void(bool accepted)>& onDone);
};

#endif // ADDRESSBOOKPAGE_H
