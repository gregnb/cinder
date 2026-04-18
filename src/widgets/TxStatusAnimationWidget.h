#ifndef TXSTATUSANIMATIONWIDGET_H
#define TXSTATUSANIMATIONWIDGET_H

#include <QVariantAnimation>
#include <QWidget>

class TxStatusAnimationWidget : public QWidget {
    Q_OBJECT

  public:
    enum class State { Confirming, Confirmed, Finalized, Failed };

    explicit TxStatusAnimationWidget(QWidget* parent = nullptr);

    void setState(State state);
    void reset();

  protected:
    void paintEvent(QPaintEvent* event) override;

  private:
    void stopAllAnimations();
    void startSpinAnimation();
    void startCompletionAnimation();
    void startIconAnimation();
    void startGlowAnimation();

    State m_state = State::Confirming;
    bool m_failed = false;

    QVariantAnimation* m_spinAnim = nullptr;
    QVariantAnimation* m_completionAnim = nullptr;
    QVariantAnimation* m_iconAnim = nullptr;
    QVariantAnimation* m_glowAnim = nullptr;

    qreal m_spinAngle = 0.0;
    qreal m_arcSpan = 0.0;
    qreal m_fillOpacity = 0.0;
    qreal m_iconProgress = 0.0;
    qreal m_glowOpacity = 0.0;
};

#endif // TXSTATUSANIMATIONWIDGET_H
