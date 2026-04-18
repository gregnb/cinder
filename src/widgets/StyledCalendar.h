#ifndef STYLEDCALENDAR_H
#define STYLEDCALENDAR_H

#include <QCalendarWidget>
#include <QDate>

class QAbstractItemView;

class StyledCalendar : public QCalendarWidget {
    Q_OBJECT
  public:
    explicit StyledCalendar(QWidget* parent = nullptr);

  protected:
    void paintCell(QPainter* painter, const QRect& rect, QDate date) const override;
    bool eventFilter(QObject* obj, QEvent* event) override;

  private:
    QDate dateForIndex(const QModelIndex& idx) const;
    QAbstractItemView* m_view = nullptr;
    mutable QDate m_hoveredDate;
};

#endif // STYLEDCALENDAR_H
