#ifndef AGENTSPAGE_H
#define AGENTSPAGE_H

#include "db/McpDb.h"
#include "features/agents/AgentsHandler.h"
#include "features/agents/ApprovalExecutor.h"
#include <QHash>
#include <QList>
#include <QMap>
#include <QSet>
#include <QWidget>

class QLabel;
class QLineEdit;
class QPushButton;
class QScrollArea;
class QScrollBar;
class QVBoxLayout;
class QStackedWidget;
class QTimer;
class Dropdown;
class TabBar;
class McpClient;
class ModelProvider;
class SolanaApi;
class Signer;

class AgentsPage : public QWidget {
    Q_OBJECT
  public:
    explicit AgentsPage(QWidget* parent = nullptr);

    void setWalletAddress(const QString& address);
    void setSolanaApi(SolanaApi* api);
    void setSigner(Signer* signer);
    void setSignerFactory(ApprovalExecutor::SignerFactory factory);

  signals:
    void notificationAdded();
    void contactsChanged();
    void balancesChanged();
    void stakeChanged();
    void nonceAccountsChanged();

  protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

  private:
    enum class PolicyStep { List = 0, Detail };
    void showPolicyStep(PolicyStep step);

    // Tab content builders
    QWidget* buildPoliciesTab();
    QWidget* buildApprovalsTab();
    QWidget* buildConfigurationTab();
    QWidget* buildActivityTab();

    // Policy list/detail helpers
    void refreshPolicyList();
    void showPolicyDetail(int policyId);
    void showPolicyList();
    void onCreatePolicy();
    void onDeletePolicy(int policyId);
    void onRegenerateKey(int policyId);
    QString policyPermissionSummary(int policyId);
    bool confirmFundRiskAllow(const QString& toolName);
    bool confirmFundRiskAllowBulk(const QStringList& toolNames);

    // Configuration tab helpers
    QWidget* buildProviderCard(ModelProvider* provider);
    QWidget* buildCodeBlock(const QString& text, const QString& language);

    void onTestClicked();
    void onTestComplete(bool success, const QString& summary);

    void rebuildConfigSnippets();

    void refreshApprovals();
    void onApprove(const QString& id);
    void onReject(const QString& id);

    // Activity tab — virtualized list
    QWidget* createActivityPoolRow();
    void bindActivityRow(QWidget* row, int cacheIndex);
    void relayoutActivityRows();
    void showResultDialog(const QString& jsonResult);
    void loadInitialActivity();
    void loadMoreActivity();
    void pollActivityUpdates();
    void refreshPolicyNameCache();

    McpClient* m_mcpClient = nullptr;
    QString m_mcpBinaryPath;
    QString m_walletAddress;

    SolanaApi* m_solanaApi = nullptr;
    Signer* m_signer = nullptr;

    // Tabs
    TabBar* m_tabs = nullptr;
    QStackedWidget* m_tabStack = nullptr;

    // Access Policies tab
    QLabel* m_serverStatus = nullptr;
    QLabel* m_serverPath = nullptr;
    QLabel* m_testResult = nullptr;
    QPushButton* m_testBtn = nullptr;
    QStackedWidget* m_policyStack = nullptr;
    QVBoxLayout* m_policyDetailLayout = nullptr;
    QVBoxLayout* m_policyListLayout = nullptr;

    // Create policy form
    QLineEdit* m_createNameInput = nullptr;

    // Configuration tab
    QList<ModelProvider*> m_providers;
    QMap<QString, QWidget*> m_codeBlocks; // provider id -> code block widget
    Dropdown* m_configPolicyDropdown = nullptr;
    QList<AgentPolicyCard> m_configPolicies;

    // Approvals tab
    QVBoxLayout* m_approvalsLayout = nullptr;
    QTimer* m_approvalTimer = nullptr;
    QLabel* m_approvalsBadge = nullptr;

    // Activity tab — virtualized list
    static constexpr int ACTIVITY_ROW_H = 30;
    static constexpr int ACTIVITY_BUFFER = 50;
    static constexpr int ACTIVITY_POOL_SIZE = 180;
    static constexpr int ACTIVITY_PAGE_SIZE = 200;

    QPushButton* m_activityClearBtn = nullptr;
    QWidget* m_activityTableHeader = nullptr;
    QStackedWidget* m_activityBodyStack = nullptr;
    QScrollArea* m_activityScroll = nullptr;
    QWidget* m_activityContainer = nullptr;
    QList<QWidget*> m_activityRowPool;
    QHash<int, QWidget*> m_activeActivityRows;
    QList<McpActivityRecord> m_activityCache;
    int m_activityTotalCount = 0;
    bool m_activityAllLoaded = false;
    QLabel* m_activityEmpty = nullptr;
    QHash<int, QString> m_policyNameCache;
    QTimer* m_activityTimer = nullptr;
    AgentsHandler m_handler;
    ApprovalExecutor* m_executor = nullptr;
    QSet<QString> m_notifiedApprovalIds;
};

#endif // AGENTSPAGE_H
