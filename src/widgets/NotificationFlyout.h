#ifndef NOTIFICATIONFLYOUT_H
#define NOTIFICATIONFLYOUT_H

#include "db/NotificationDb.h"
#include <QList>
#include <QWidget>

class QLabel;
class QScrollArea;
class QVBoxLayout;

class NotificationFlyout : public QWidget {
    Q_OBJECT
  public:
    explicit NotificationFlyout(QWidget* parent = nullptr);

    void showFlyout();
    void hideFlyout();
    void reload();

  signals:
    void allMarkedRead();
    void badgeChanged();

  protected:
    bool eventFilter(QObject* obj, QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

  private:
    void buildList(const QList<NotificationRecord>& notifications);
    QWidget* createNotificationRow(const NotificationRecord& n);

    QScrollArea* m_scrollArea = nullptr;
    QVBoxLayout* m_listLayout = nullptr;
    QLabel* m_emptyLabel = nullptr;
};

#endif // NOTIFICATIONFLYOUT_H
