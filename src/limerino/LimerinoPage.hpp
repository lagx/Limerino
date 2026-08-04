#pragma once

#include "widgets/settingspages/SettingsPage.hpp"

class QPushButton;
class QListWidget;

namespace chatterino {

class DescriptionLabel;
class GeneralPageView;

class LimerinoPage : public SettingsPage
{
    Q_OBJECT

public:
    LimerinoPage();

    bool filterElements(const QString &query) override;
    void onShow() override;

private:
    void initLayout(GeneralPageView &layout);
    void rebuildAuthSummary();
    void rebuildPubSubDiagnostics();
    void rebuildAutoActionList();

    GeneralPageView *view_{};
    DescriptionLabel *authSummaryLabel_{};
    DescriptionLabel *pubsubSummaryLabel_{};
    DescriptionLabel *pubsubDetailLabel_{};
    QPushButton *autoActionAdd_{};
    QPushButton *autoActionEdit_{};
    QPushButton *autoActionDelete_{};
    QListWidget *autoActionList_{};
    DescriptionLabel *autoActionPlaceholderHelp_{};
};

}  // namespace chatterino
