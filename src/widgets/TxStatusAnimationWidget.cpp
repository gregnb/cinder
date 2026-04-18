#include "widgets/TxStatusAnimationWidget.h"

#include <QPainter>

namespace {
    constexpr qreal kArcSpanConfirming = 100.0;
    constexpr int kSpinDurationMs = 1200;
    constexpr int kCompletionDurationMs = 800;
    constexpr int kIconDurationMs = 500;
    constexpr int kGlowDurationMs = 600;

    const QColor kColorGreen(52, 211, 153);
    const QColor kColorRed(239, 68, 68);

    constexpr int kWidgetSizePx = 160;
} // namespace

TxStatusAnimationWidget::TxStatusAnimationWidget(QWidget* parent) : QWidget(parent) {
    setAttribute(Qt::WA_TranslucentBackground);
    setFixedSize(kWidgetSizePx, kWidgetSizePx);
    reset();
}

void TxStatusAnimationWidget::stopAllAnimations() {
    auto stop = [](QVariantAnimation*& anim) {
        if (anim) {
            anim->disconnect();
            anim->stop();
            anim->deleteLater();
            anim = nullptr;
        }
    };
    stop(m_spinAnim);
    stop(m_completionAnim);
    stop(m_iconAnim);
    stop(m_glowAnim);
}

void TxStatusAnimationWidget::reset() {
    stopAllAnimations();
    m_state = State::Confirming;
    m_failed = false;
    m_spinAngle = 0.0;
    m_arcSpan = kArcSpanConfirming;
    m_fillOpacity = 0.0;
    m_iconProgress = 0.0;
    m_glowOpacity = 0.0;
    update();
    startSpinAnimation();
}

void TxStatusAnimationWidget::setState(State state) {
    if (m_state == state) {
        return;
    }
    const State prev = m_state;
    m_state = state;

    switch (state) {
        case State::Confirming:
            reset();
            break;

        case State::Confirmed:
            if (m_spinAnim) {
                m_spinAnim->disconnect();
                m_spinAnim->stop();
                m_spinAnim->deleteLater();
                m_spinAnim = nullptr;
            }
            startCompletionAnimation();
            connect(
                m_completionAnim, &QVariantAnimation::finished, this,
                [this]() {
                    startIconAnimation();
                    startGlowAnimation();
                },
                Qt::SingleShotConnection);
            break;

        case State::Finalized:
            // No additional animation — icon already complete from Confirmed
            break;

        case State::Failed:
            m_failed = true;
            if (prev == State::Confirming) {
                if (m_spinAnim) {
                    m_spinAnim->disconnect();
                    m_spinAnim->stop();
                    m_spinAnim->deleteLater();
                    m_spinAnim = nullptr;
                }
                startCompletionAnimation();
                connect(
                    m_completionAnim, &QVariantAnimation::finished, this,
                    [this]() {
                        startIconAnimation();
                        startGlowAnimation();
                    },
                    Qt::SingleShotConnection);
            } else if (m_completionAnim &&
                       m_completionAnim->state() == QAbstractAnimation::Running) {
                connect(
                    m_completionAnim, &QVariantAnimation::finished, this,
                    [this]() {
                        startIconAnimation();
                        startGlowAnimation();
                    },
                    Qt::SingleShotConnection);
            } else {
                startIconAnimation();
                startGlowAnimation();
            }
            update();
            break;
    }
}

void TxStatusAnimationWidget::startSpinAnimation() {
    m_spinAnim = new QVariantAnimation(this);
    m_spinAnim->setStartValue(0.0);
    m_spinAnim->setEndValue(360.0);
    m_spinAnim->setDuration(kSpinDurationMs);
    m_spinAnim->setLoopCount(-1);
    m_spinAnim->setEasingCurve(QEasingCurve::Linear);
    connect(m_spinAnim, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) {
        m_spinAngle = v.toDouble();
        update();
    });
    m_spinAnim->start();
}

void TxStatusAnimationWidget::startCompletionAnimation() {
    m_completionAnim = new QVariantAnimation(this);
    m_completionAnim->setStartValue(0.0);
    m_completionAnim->setEndValue(1.0);
    m_completionAnim->setDuration(kCompletionDurationMs);
    m_completionAnim->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_completionAnim, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) {
        const qreal t = v.toDouble();
        m_arcSpan = kArcSpanConfirming + (360.0 - kArcSpanConfirming) * t;
        m_fillOpacity = t;
        update();
    });
    m_completionAnim->start();
}

void TxStatusAnimationWidget::startIconAnimation() {
    m_iconAnim = new QVariantAnimation(this);
    m_iconAnim->setStartValue(0.0);
    m_iconAnim->setEndValue(1.0);
    m_iconAnim->setDuration(kIconDurationMs);
    m_iconAnim->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_iconAnim, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) {
        m_iconProgress = v.toDouble();
        update();
    });
    m_iconAnim->start();
}

void TxStatusAnimationWidget::startGlowAnimation() {
    m_glowAnim = new QVariantAnimation(this);
    m_glowAnim->setStartValue(0.0);
    m_glowAnim->setEndValue(1.0);
    m_glowAnim->setDuration(kGlowDurationMs);
    m_glowAnim->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_glowAnim, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) {
        m_glowOpacity = v.toDouble();
        update();
    });
    m_glowAnim->start();
}

void TxStatusAnimationWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const qreal w = width();
    const qreal h = height();
    const qreal cx = w / 2.0;
    const qreal cy = h / 2.0;
    const qreal radius = qMin(w, h) * 0.32;
    const qreal strokeWidth = qMin(w, h) * 0.055;
    const qreal glowRadius = radius + strokeWidth * 2.5;

    const QColor mainColor = m_failed ? kColorRed : kColorGreen;

    const QRectF circleRect(cx - radius, cy - radius, radius * 2, radius * 2);

    // 1. Outer glow ring
    if (m_glowOpacity > 0.0) {
        QColor glowColor = mainColor;
        glowColor.setAlphaF(m_glowOpacity * 0.25);
        QPen glowPen(glowColor, strokeWidth * 0.5);
        p.setPen(glowPen);
        p.setBrush(Qt::NoBrush);
        const QRectF glowRect(cx - glowRadius, cy - glowRadius, glowRadius * 2, glowRadius * 2);
        p.drawEllipse(glowRect);
    }

    // 2. Gray track circle (fades as fill appears)
    if (m_fillOpacity < 1.0) {
        QColor trackColor(255, 255, 255, static_cast<int>(25 * (1.0 - m_fillOpacity)));
        QPen trackPen(trackColor, strokeWidth, Qt::SolidLine, Qt::RoundCap);
        p.setPen(trackPen);
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(circleRect);
    }

    // 3. Filled circle
    if (m_fillOpacity > 0.0) {
        QColor fillColor = mainColor;
        fillColor.setAlphaF(m_fillOpacity);
        p.setPen(Qt::NoPen);
        p.setBrush(fillColor);
        p.drawEllipse(circleRect);
    }

    // 4. Arc stroke (hidden once circle is fully filled)
    if (m_arcSpan < 359.5) {
        QPen arcPen(mainColor, strokeWidth, Qt::SolidLine, Qt::RoundCap);
        p.setPen(arcPen);
        p.setBrush(Qt::NoBrush);
        // Convert: our 0°=top CW → Qt 0°=3-o'clock CCW
        const int startAngle16 = static_cast<int>((90.0 - m_spinAngle) * 16.0);
        const int spanAngle16 = static_cast<int>(-m_arcSpan * 16.0);
        p.drawArc(circleRect, startAngle16, spanAngle16);
    }

    // 5. Icon (checkmark or X)
    if (m_iconProgress > 0.0) {
        QPen iconPen(Qt::white, strokeWidth * 0.85, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        p.setPen(iconPen);

        if (!m_failed) {
            // Checkmark: short leg then long leg
            const QPointF p1(cx - radius * 0.32, cy + radius * 0.02);
            const QPointF p2(cx - radius * 0.08, cy + radius * 0.30);
            const QPointF p3(cx + radius * 0.38, cy - radius * 0.28);

            constexpr qreal kLeg1End = 0.35;
            if (m_iconProgress <= kLeg1End) {
                const qreal leg = m_iconProgress / kLeg1End;
                p.drawLine(p1, p1 + (p2 - p1) * leg);
            } else {
                const qreal leg = (m_iconProgress - kLeg1End) / (1.0 - kLeg1End);
                p.drawLine(p1, p2);
                p.drawLine(p2, p2 + (p3 - p2) * leg);
            }
        } else {
            // X: two diagonal strokes
            const qreal off = radius * 0.28;
            const QPointF tl(cx - off, cy - off);
            const QPointF br(cx + off, cy + off);
            const QPointF tr(cx + off, cy - off);
            const QPointF bl(cx - off, cy + off);

            if (m_iconProgress <= 0.5) {
                const qreal leg = m_iconProgress / 0.5;
                p.drawLine(tl, tl + (br - tl) * leg);
            } else {
                const qreal leg = (m_iconProgress - 0.5) / 0.5;
                p.drawLine(tl, br);
                p.drawLine(tr, tr + (bl - tr) * leg);
            }
        }
    }
}
