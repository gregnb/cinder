#ifndef STAKESTATUSRAIL_H
#define STAKESTATUSRAIL_H

#include "features/staking/StakeLifecycle.h"
#include <QWidget>
#include <array>

class QLabel;
class QFrame;
class QTimer;

class StakeStatusRail : public QWidget {
    Q_OBJECT

  public:
    explicit StakeStatusRail(QWidget* parent = nullptr);

    void setLifecycle(const StakeLifecycle& lifecycle);
    void setCurrentEtaText(const QString& text);
    void setFooterText(const QString& text);
    void setFooterPending(bool pending, const QString& text = {});

  protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

  private:
    struct NodeWidgets {
        QWidget* container = nullptr;
        QFrame* dot = nullptr;
        QLabel* title = nullptr;
        QLabel* subtitle = nullptr;
    };

    void applyNodeState(NodeWidgets& node, const QString& state);
    void applyConnectorState(QFrame* connector, const QString& state);
    QString subtitleForNode(int index) const;

    QLabel* m_descriptionLabel = nullptr;
    std::array<NodeWidgets, 3> m_nodes;
    std::array<QFrame*, 2> m_connectors = {nullptr, nullptr};
    StakeLifecycle m_lifecycle;
    QString m_currentEtaText;
    QString m_footerText;
    QWidget* m_footerBadge = nullptr;
    QLabel* m_footerBadgeText = nullptr;
    QWidget* m_footerSpinner = nullptr;
    QTimer* m_footerSpinnerTimer = nullptr;
    int m_footerSpinnerAngle = 0;
    bool m_footerPending = false;
};

#endif // STAKESTATUSRAIL_H
