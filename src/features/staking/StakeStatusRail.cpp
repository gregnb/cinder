#include "features/staking/StakeStatusRail.h"
#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPen>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>

namespace {

    bool isUnstakeFlow(StakeLifecycle::Phase phase) {
        return phase == StakeLifecycle::Phase::Deactivating ||
               phase == StakeLifecycle::Phase::Inactive;
    }

    QString phaseTitle(int index, const StakeLifecycle& lifecycle) {
        if (isUnstakeFlow(lifecycle.phase)) {
            switch (index) {
                case 0:
                    return QObject::tr("Unstake Requested");
                case 1:
                    return QObject::tr("Cooldown");
                case 2:
                    return QObject::tr("Withdrawable");
                default:
                    return QString();
            }
        }

        switch (index) {
            case 0:
                return QObject::tr("Stake Requested");
            case 1:
                return QObject::tr("Activated");
            case 2:
                return QObject::tr("Earning");
            default:
                return QString();
        }
    }

    QString phaseSubtitle(int index, const StakeLifecycle& lifecycle) {
        if (isUnstakeFlow(lifecycle.phase)) {
            switch (index) {
                case 0:
                    return QObject::tr("submitted");
                case 1:
                    return lifecycle.phase == StakeLifecycle::Phase::Deactivating
                               ? QObject::tr("in progress")
                               : QObject::tr("complete");
                case 2:
                    return lifecycle.phase == StakeLifecycle::Phase::Inactive
                               ? (lifecycle.canWithdraw ? QObject::tr("ready")
                                                        : QObject::tr("locked"))
                               : QObject::tr("next");
                default:
                    return QString();
            }
        }

        switch (index) {
            case 0:
                return QObject::tr("submitted");
            case 1:
                return lifecycle.phase == StakeLifecycle::Phase::Activating
                           ? QObject::tr("in progress")
                           : QObject::tr("complete");
            case 2:
                return QObject::tr("earning");
            default:
                return QString();
        }
    }

} // namespace

QString StakeStatusRail::subtitleForNode(int index) const {
    if (index == m_lifecycle.currentStepIndex && !m_currentEtaText.isEmpty() &&
        (m_lifecycle.phase == StakeLifecycle::Phase::Activating ||
         m_lifecycle.phase == StakeLifecycle::Phase::Deactivating)) {
        return m_currentEtaText;
    }
    return phaseSubtitle(index, m_lifecycle);
}

StakeStatusRail::StakeStatusRail(QWidget* parent) : QWidget(parent) {
    setObjectName("stakingTransparent");
    setAttribute(Qt::WA_StyledBackground, true);

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(10);

    auto* rail = new QHBoxLayout();
    rail->setContentsMargins(0, 0, 0, 0);
    rail->setSpacing(10);

    for (int i = 0; i < static_cast<int>(m_nodes.size()); ++i) {
        auto& node = m_nodes[i];
        node.container = new QWidget();
        node.container->setObjectName("stakingLifecycleNode");
        node.container->setAttribute(Qt::WA_StyledBackground, true);

        auto* nodeLayout = new QVBoxLayout(node.container);
        nodeLayout->setContentsMargins(0, 0, 0, 0);
        nodeLayout->setSpacing(6);

        node.dot = new QFrame();
        node.dot->setObjectName("stakingLifecycleDot");
        node.dot->setFixedSize(12, 12);
        node.dot->setProperty("railState", "upcoming");
        node.dot->setAttribute(Qt::WA_StyledBackground, true);
        nodeLayout->addWidget(node.dot, 0, Qt::AlignHCenter);

        node.title = new QLabel();
        node.title->setObjectName("stakingLifecycleNodeTitle");
        node.title->setAlignment(Qt::AlignHCenter);
        nodeLayout->addWidget(node.title);

        node.subtitle = new QLabel();
        node.subtitle->setObjectName("stakingLifecycleNodeSubtitle");
        node.subtitle->setAlignment(Qt::AlignHCenter);
        nodeLayout->addWidget(node.subtitle);

        rail->addWidget(node.container, 1);

        if (i < static_cast<int>(m_connectors.size())) {
            auto* connector = new QFrame();
            connector->setObjectName("stakingLifecycleConnector");
            connector->setFixedHeight(2);
            connector->setProperty("railState", "upcoming");
            connector->setAttribute(Qt::WA_StyledBackground, true);
            m_connectors[i] = connector;
            rail->addWidget(connector, 1);
        }
    }

    outer->addLayout(rail);

    auto* footerRow = new QHBoxLayout();
    footerRow->setContentsMargins(0, 0, 0, 0);
    footerRow->setSpacing(8);

    m_descriptionLabel = new QLabel();
    m_descriptionLabel->setObjectName("stakingLifecycleDescription");
    m_descriptionLabel->setWordWrap(true);
    footerRow->addWidget(m_descriptionLabel, 1);

    m_footerBadge = new QWidget();
    m_footerBadge->setAttribute(Qt::WA_StyledBackground, true);
    m_footerBadge->setFixedHeight(22);
    m_footerBadge->setProperty("uiClass", "srSuccessBadge");
    m_footerBadge->setProperty("tone", "confirming");
    m_footerBadge->hide();

    auto* badgeLay = new QHBoxLayout(m_footerBadge);
    badgeLay->setContentsMargins(8, 0, 6, 0);
    badgeLay->setSpacing(4);

    m_footerBadgeText = new QLabel();
    m_footerBadgeText->setProperty("uiClass", "srSuccessResultText");
    m_footerBadgeText->setProperty("tone", "confirming");
    badgeLay->addWidget(m_footerBadgeText, 0, Qt::AlignVCenter);

    m_footerSpinner = new QWidget();
    m_footerSpinner->setFixedSize(10, 10);
    m_footerSpinner->setObjectName("srSuccessSpinner");
    m_footerSpinner->installEventFilter(this);
    badgeLay->addWidget(m_footerSpinner, 0, Qt::AlignVCenter);

    footerRow->addWidget(m_footerBadge, 0, Qt::AlignLeft);
    outer->addLayout(footerRow);

    m_footerSpinnerTimer = new QTimer(this);
    m_footerSpinnerTimer->setInterval(30);
    connect(m_footerSpinnerTimer, &QTimer::timeout, this, [this]() {
        m_footerSpinnerAngle = (m_footerSpinnerAngle + 12) % 360;
        if (m_footerSpinner && m_footerSpinner->isVisible()) {
            m_footerSpinner->update();
        }
    });
}

bool StakeStatusRail::eventFilter(QObject* obj, QEvent* event) {
    if (obj == m_footerSpinner && event->type() == QEvent::Paint) {
        QPainter p(m_footerSpinner);
        p.setRenderHint(QPainter::Antialiasing);
        QPen pen(QColor(253, 214, 99, 200), 2.0, Qt::SolidLine, Qt::RoundCap);
        p.setPen(pen);
        QRectF r(2.0, 2.0, m_footerSpinner->width() - 4.0, m_footerSpinner->height() - 4.0);
        p.drawArc(r, m_footerSpinnerAngle * 16, 270 * 16);
        return true;
    }
    return QWidget::eventFilter(obj, event);
}

void StakeStatusRail::applyNodeState(NodeWidgets& node, const QString& state) {
    node.container->setProperty("railState", state);
    node.dot->setProperty("railState", state);
    node.title->setProperty("railState", state);
    node.subtitle->setProperty("railState", state);
    node.container->style()->unpolish(node.container);
    node.container->style()->polish(node.container);
    node.dot->style()->unpolish(node.dot);
    node.dot->style()->polish(node.dot);
    node.title->style()->unpolish(node.title);
    node.title->style()->polish(node.title);
    node.subtitle->style()->unpolish(node.subtitle);
    node.subtitle->style()->polish(node.subtitle);
}

void StakeStatusRail::applyConnectorState(QFrame* connector, const QString& state) {
    connector->setProperty("railState", state);
    connector->style()->unpolish(connector);
    connector->style()->polish(connector);
}

void StakeStatusRail::setLifecycle(const StakeLifecycle& lifecycle) {
    m_lifecycle = lifecycle;
    const bool transitional = lifecycle.phase == StakeLifecycle::Phase::Activating ||
                              lifecycle.phase == StakeLifecycle::Phase::Deactivating;
    const QString footerText =
        !m_footerText.isEmpty() ? m_footerText : (transitional ? QString() : lifecycle.description);
    m_descriptionLabel->setVisible(!m_footerPending && !footerText.isEmpty());
    m_descriptionLabel->setText(footerText);
    m_footerBadge->setVisible(m_footerPending);

    for (int i = 0; i < static_cast<int>(m_nodes.size()); ++i) {
        m_nodes[i].title->setText(phaseTitle(i, lifecycle));
        m_nodes[i].subtitle->setText(subtitleForNode(i));
        const QString state = i < lifecycle.currentStepIndex
                                  ? QStringLiteral("complete")
                                  : (i == lifecycle.currentStepIndex ? QStringLiteral("current")
                                                                     : QStringLiteral("upcoming"));
        applyNodeState(m_nodes[i], state);
    }

    for (int i = 0; i < static_cast<int>(m_connectors.size()); ++i) {
        const QString state = i < lifecycle.currentStepIndex ? QStringLiteral("complete")
                                                             : QStringLiteral("upcoming");
        applyConnectorState(m_connectors[i], state);
    }
}

void StakeStatusRail::setCurrentEtaText(const QString& text) {
    if (m_currentEtaText == text) {
        return;
    }
    m_currentEtaText = text;
    for (int i = 0; i < static_cast<int>(m_nodes.size()); ++i) {
        m_nodes[i].title->setText(phaseTitle(i, m_lifecycle));
        m_nodes[i].subtitle->setText(subtitleForNode(i));
    }
}

void StakeStatusRail::setFooterText(const QString& text) {
    if (m_footerText == text) {
        return;
    }
    m_footerText = text;
    const bool transitional = m_lifecycle.phase == StakeLifecycle::Phase::Activating ||
                              m_lifecycle.phase == StakeLifecycle::Phase::Deactivating;
    const QString footerText = !m_footerText.isEmpty()
                                   ? m_footerText
                                   : (transitional ? QString() : m_lifecycle.description);
    m_descriptionLabel->setVisible(!m_footerPending && !footerText.isEmpty());
    m_descriptionLabel->setText(footerText);
}

void StakeStatusRail::setFooterPending(bool pending, const QString& text) {
    if (m_footerPending == pending &&
        (!pending || (m_footerBadgeText && m_footerBadgeText->text() == text))) {
        return;
    }

    m_footerPending = pending;
    if (m_footerBadgeText) {
        m_footerBadgeText->setText(text.isEmpty() ? tr("Estimating Reward") : text);
    }

    if (m_footerBadge) {
        m_footerBadge->setVisible(pending);
    }

    const bool transitional = m_lifecycle.phase == StakeLifecycle::Phase::Activating ||
                              m_lifecycle.phase == StakeLifecycle::Phase::Deactivating;
    const QString footerText = !m_footerText.isEmpty()
                                   ? m_footerText
                                   : (transitional ? QString() : m_lifecycle.description);
    m_descriptionLabel->setVisible(!pending && !footerText.isEmpty());
    m_descriptionLabel->setText(footerText);

    if (pending) {
        if (!m_footerSpinnerTimer->isActive()) {
            m_footerSpinnerTimer->start();
        }
    } else {
        m_footerSpinnerTimer->stop();
    }
}
