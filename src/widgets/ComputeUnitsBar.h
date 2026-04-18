#ifndef COMPUTEUNITSBAR_H
#define COMPUTEUNITSBAR_H

#include <QColor>
#include <QList>
#include <QWidget>

class QLabel;

class ComputeUnitsBar : public QWidget {
  public:
    struct Segment {
        QString label;
        int units;
        QColor color;
    };

    explicit ComputeUnitsBar(QWidget* parent = nullptr);
    ~ComputeUnitsBar() override;

    void setSegments(const QList<Segment>& segments);
    int total() const;
    const QList<Segment>& segments() const;

  protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;

  private:
    int segmentAt(int x) const;
    void ensurePopup();
    void showPopup(const QPoint& globalPos, const Segment& seg);
    void hidePopup();

    QList<Segment> m_segments;
    int m_total = 0;
    int m_hoveredIndex = -1;
    QWidget* m_popup = nullptr;
    QLabel* m_popupDot = nullptr;
    QLabel* m_popupName = nullptr;
    QLabel* m_popupValue = nullptr;
};

#endif // COMPUTEUNITSBAR_H
