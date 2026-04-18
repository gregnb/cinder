#ifndef FLAPDISPLAY_H
#define FLAPDISPLAY_H

#include <QChar>
#include <QPropertyAnimation>
#include <QWidget>

class QHBoxLayout;

// ── Single character tile with split-flap animation ─────────────

class FlapDigit : public QWidget {
    Q_OBJECT
    Q_PROPERTY(qreal flipProgress READ flipProgress WRITE setFlipProgress)

  public:
    explicit FlapDigit(QChar initial = QChar('-'), QWidget* parent = nullptr);

    void setChar(QChar c);
    void setAnimated(bool animated) { m_animated = animated; }
    QChar currentChar() const { return m_current; }

    qreal flipProgress() const { return m_flipProgress; }
    void setFlipProgress(qreal p);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

  protected:
    void paintEvent(QPaintEvent* event) override;

  private:
    void startFlip();

    QChar m_current{'-'};
    QChar m_previous{'-'};
    qreal m_flipProgress = 1.0; // 1.0 = idle (showing current)
    int m_rollSteps = 1;        // number of intermediate digits to roll through
    QPropertyAnimation* m_anim = nullptr;
    bool m_animated = true;
};

// ── Row of FlapDigit tiles ──────────────────────────────────────

class FlapDisplay : public QWidget {
    Q_OBJECT

  public:
    explicit FlapDisplay(QWidget* parent = nullptr);

    void setValue(const QString& formatted);
    void setAnimated(bool animated);
    QString value() const { return m_text; }

  private:
    void ensureDigitCount(int count);

    QHBoxLayout* m_layout = nullptr;
    QList<FlapDigit*> m_digits;
    QString m_text;
    bool m_animated = true;
};

#endif // FLAPDISPLAY_H
