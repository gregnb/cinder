#ifndef DROPDOWN_H
#define DROPDOWN_H

#include <QIcon>
#include <QWidget>

class QPushButton;
class QListWidget;
class QListWidgetItem;
class QLabel;

class Dropdown : public QWidget {
    Q_OBJECT
  public:
    explicit Dropdown(QWidget* parent = nullptr);
    ~Dropdown() override;

    void addItem(const QString& text);
    void addItem(const QIcon& icon, const QString& text);
    void clear();
    void setCurrentItem(const QString& text);
    QString currentText() const;

  signals:
    void itemSelected(const QString& text);

  protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

  private:
    QPushButton* m_button = nullptr;
    QWidget* m_popup = nullptr;
    QListWidget* m_list = nullptr;
    QLabel* m_label = nullptr;
    QLabel* m_chevron = nullptr;

    void showDropdown();
    void hideDropdown();
    void updateChevron(bool open);
};

#endif // DROPDOWN_H
