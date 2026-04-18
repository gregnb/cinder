#ifndef TABBAR_H
#define TABBAR_H

#include <QHBoxLayout>
#include <QLabel>
#include <QList>
#include <QPainter>
#include <QPropertyAnimation>
#include <QWidget>

// Reusable horizontal tab bar with an animated underline indicator.
// Each tab is a clickable QLabel with hover and selected states.
// The active indicator slides smoothly between tabs.

class TabBar : public QWidget {
    Q_OBJECT
    Q_PROPERTY(qreal indicatorX READ indicatorX WRITE setIndicatorX)
    Q_PROPERTY(qreal indicatorWidth READ indicatorWidth WRITE setIndicatorWidth)

  public:
    explicit TabBar(QWidget* parent = nullptr);

    void addTab(const QString& text);
    void setTabText(int index, const QString& text);
    void setActiveIndex(int index);
    int activeIndex() const { return m_activeIndex; }
    int count() const { return m_tabs.size(); }

    // Whether tabs stretch to fill width (default: true)
    void setStretch(bool stretch);

  signals:
    void currentChanged(int index);

  protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;

  private:
    void updateStyles();
    void animateIndicator(int index);

    qreal indicatorX() const { return m_indicatorX; }
    void setIndicatorX(qreal x);
    qreal indicatorWidth() const { return m_indicatorW; }
    void setIndicatorWidth(qreal w);

    QHBoxLayout* m_layout = nullptr;
    QList<QLabel*> m_tabs;
    int m_activeIndex = -1;
    int m_hoveredIndex = -1;
    bool m_stretch = true;

    // Indicator animation
    qreal m_indicatorX = 0;
    qreal m_indicatorW = 0;
    QPropertyAnimation* m_animX = nullptr;
    QPropertyAnimation* m_animW = nullptr;
};

#endif // TABBAR_H
