#ifndef AMOUNTINPUT_H
#define AMOUNTINPUT_H

#include <QLineEdit>
#include <QPushButton>
#include <QWidget>

class AmountInput : public QWidget {
    Q_OBJECT
  public:
    explicit AmountInput(QWidget* parent = nullptr);

    QString text() const;
    void setText(const QString& text);
    void clear();
    void setPlaceholderText(const QString& text);
    QLineEdit* lineEdit() const;

  signals:
    void textChanged(const QString& text);
    void maxClicked();

  private:
    QLineEdit* m_input = nullptr;
    QPushButton* m_maxBtn = nullptr;
};

#endif // AMOUNTINPUT_H
