#include "FlapDisplay.h"
#include "Theme.h"
#include <QHBoxLayout>
#include <QPainter>
#include <QTimer>

// ── Constants ───────────────────────────────────────────────────

static const int DIGIT_W = 9;
static const int DIGIT_H = 20;
static const int COMMA_W = 5;
static const int SINGLE_FLIP_MS = 220; // duration of one digit-to-digit flip
static const int STAGGER_MS = 40;

static const QColor textColor(255, 255, 255);
static const QColor textDimColor(255, 255, 255, 100);

static QFont digitFont() {
    QFont f(Theme::fontFamily, 14);
    f.setWeight(QFont::DemiBold);
    return f;
}

// ── Helpers ─────────────────────────────────────────────────────

// How many digit steps from old→new rolling forward (0→1→...→9→0→...)
static int digitDistance(QChar from, QChar to) {
    if (!from.isDigit() || !to.isDigit()) {
        return 1;
    }
    int a = from.digitValue();
    int b = to.digitValue();
    return (b - a + 10) % 10;
}

// The intermediate digit at a given step in the roll sequence
static QChar digitAtStep(QChar from, int step) {
    if (!from.isDigit()) {
        return from;
    }
    return QChar('0' + (from.digitValue() + step) % 10);
}

// ── FlapDigit ───────────────────────────────────────────────────

FlapDigit::FlapDigit(QChar initial, QWidget* parent)
    : QWidget(parent), m_current(initial), m_previous(initial) {
    setAttribute(Qt::WA_OpaquePaintEvent, false);
    setAttribute(Qt::WA_TranslucentBackground);
    int w = (initial == ',' || initial == ' ') ? COMMA_W : DIGIT_W;
    setFixedSize(w, DIGIT_H);
}

void FlapDigit::setChar(QChar c) {
    if (c == m_current) {
        return;
    }
    if (m_anim) {
        m_anim->stop();
    }
    m_previous = m_current;
    m_current = c;

    // Resize: commas and spaces are narrower, letters get digit width
    setFixedWidth((c == ',' || c == ' ') ? COMMA_W : DIGIT_W);

    if (!m_animated) {
        m_flipProgress = 1.0;
        update();
        return;
    }

    // Only digit→digit transitions roll; everything else is instant
    if (!c.isDigit() || !m_previous.isDigit()) {
        m_flipProgress = 1.0;
        update();
        return;
    }

    startFlip();
}

void FlapDigit::startFlip() {
    // Total steps to roll through (e.g., 3→7 = 4 steps: 3→4→5→6→7)
    m_rollSteps = digitDistance(m_previous, m_current);
    if (m_rollSteps == 0) {
        m_rollSteps = 1;
    }

    int totalMs = m_rollSteps * SINGLE_FLIP_MS;

    if (!m_anim) {
        m_anim = new QPropertyAnimation(this, "flipProgress", this);
        m_anim->setEasingCurve(QEasingCurve::InOutQuad);
    } else {
        m_anim->stop();
    }
    m_anim->setDuration(totalMs);
    m_anim->setStartValue(0.0);
    m_anim->setEndValue(1.0);
    m_anim->start();
}

void FlapDigit::setFlipProgress(qreal p) {
    m_flipProgress = p;
    update();
}

QSize FlapDigit::sizeHint() const {
    int w = (m_current == ',' || m_current == ' ') ? COMMA_W : DIGIT_W;
    return {w, DIGIT_H};
}

QSize FlapDigit::minimumSizeHint() const { return sizeHint(); }

void FlapDigit::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);

    const QRectF r(0, 0, width(), height());
    const qreal midY = r.height() / 2.0;
    const QRectF topHalf(0, 0, r.width(), midY);
    const QRectF botHalf(0, midY, r.width(), midY);

    p.setFont(digitFont());

    bool flipping = m_flipProgress < 1.0;

    if (!flipping) {
        // Static: draw current char
        p.setPen(textColor);
        p.drawText(r, Qt::AlignCenter, QString(m_current));
        return;
    }

    // ── Rolling animation ───────────────────────────────────────
    // Map progress [0,1] to which step we're on and how far through it.
    qreal totalProgress = m_flipProgress * m_rollSteps; // 0 → rollSteps
    int currentStep = qMin(static_cast<int>(totalProgress), m_rollSteps - 1);
    qreal stepFrac = totalProgress - currentStep; // 0→1 within this step

    QChar outgoing = digitAtStep(m_previous, currentStep);
    QChar incoming = digitAtStep(m_previous, currentStep + 1);

    // ── Top half: incoming digit slides down from above ─────────
    {
        p.save();
        p.setClipRect(topHalf);

        // Outgoing slides up and out
        qreal outY = -midY * stepFrac;
        p.setPen(QColor(255, 255, 255, static_cast<int>(255 * (1.0 - stepFrac))));
        p.drawText(r.translated(0, outY), Qt::AlignCenter, QString(outgoing));

        // Incoming slides in from below center
        qreal inY = midY * (1.0 - stepFrac);
        p.setPen(QColor(255, 255, 255, static_cast<int>(255 * stepFrac)));
        p.drawText(r.translated(0, inY), Qt::AlignCenter, QString(incoming));

        p.restore();
    }

    // ── Bottom half: same motion continues ──────────────────────
    {
        p.save();
        p.setClipRect(botHalf);

        qreal outY = -midY * stepFrac;
        p.setPen(QColor(255, 255, 255, static_cast<int>(255 * (1.0 - stepFrac))));
        p.drawText(r.translated(0, outY), Qt::AlignCenter, QString(outgoing));

        qreal inY = midY * (1.0 - stepFrac);
        p.setPen(QColor(255, 255, 255, static_cast<int>(255 * stepFrac)));
        p.drawText(r.translated(0, inY), Qt::AlignCenter, QString(incoming));

        p.restore();
    }
}

// ── FlapDisplay ─────────────────────────────────────────────────

FlapDisplay::FlapDisplay(QWidget* parent) : QWidget(parent) {
    m_layout = new QHBoxLayout(this);
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->setSpacing(0);
    setAttribute(Qt::WA_TranslucentBackground);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    setFixedHeight(DIGIT_H);
}

void FlapDisplay::setAnimated(bool animated) {
    m_animated = animated;
    for (FlapDigit* digit : m_digits) {
        digit->setAnimated(animated);
    }
}

void FlapDisplay::ensureDigitCount(int count) {
    while (m_digits.size() < count) {
        auto* d = new FlapDigit(QChar('-'), this);
        d->setAnimated(m_animated);
        m_layout->addWidget(d);
        m_digits.append(d);
    }
    while (m_digits.size() > count) {
        auto* d = m_digits.takeLast();
        m_layout->removeWidget(d);
        d->deleteLater();
    }
}

void FlapDisplay::setValue(const QString& formatted) {
    if (formatted == m_text) {
        return;
    }
    QString oldText = m_text;
    m_text = formatted;

    ensureDigitCount(formatted.size());

    // Determine which digits changed (compare right-to-left for stagger)
    QList<int> changedPositions;

    for (int i = formatted.size() - 1; i >= 0; --i) {
        QChar newCh = formatted.at(i);
        QChar oldCh = (i < oldText.size()) ? oldText.at(i) : QChar('-');
        if (newCh != oldCh) {
            changedPositions.prepend(i);
        }
    }

    // Apply characters with staggered animation start
    for (int i = 0; i < formatted.size(); ++i) {
        QChar ch = formatted.at(i);
        int delayMs = 0;

        if (changedPositions.contains(i)) {
            // Stagger: rightmost changed digit flips first
            int rightmostChanged = changedPositions.isEmpty() ? i : changedPositions.last();
            int distFromRight = rightmostChanged - i;
            delayMs = distFromRight * STAGGER_MS;
        }

        if (m_animated && delayMs > 0) {
            QTimer::singleShot(delayMs, m_digits[i],
                               [digit = m_digits[i], ch]() { digit->setChar(ch); });
        } else {
            m_digits[i]->setChar(ch);
        }
    }
}
