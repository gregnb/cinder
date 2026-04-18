#ifndef CARDLISTITEM_H
#define CARDLISTITEM_H

#include <QLabel>
#include <QPixmap>
#include <QWidget>

class CardListItem : public QWidget {
    Q_OBJECT

  public:
    explicit CardListItem(QWidget* parent = nullptr);

    // Left icon — text or pixmap, with QSS object name for styling
    // background is set inline to override viewport stylesheet cascade
    void setIcon(const QString& text, const QString& objectName, const QString& background = "");
    void setIconPixmap(const QPixmap& pixmap, const QString& objectName,
                       const QString& background = "");

    // Text content
    void setTitle(const QString& text);
    void setSubtitle(const QString& text);

    // Right-side value(s) with QSS object name for color styling
    void setValue(const QString& text, const QString& objectName);
    void setSubValue(const QString& text, const QString& objectName);

  private:
    void applyIconBackground(const QString& background);

    QLabel* m_icon;
    QLabel* m_title;
    QLabel* m_subtitle;
    QLabel* m_value;
    QLabel* m_subValue;
};

#endif // CARDLISTITEM_H
