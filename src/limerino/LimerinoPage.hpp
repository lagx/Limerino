#pragma once

#include "widgets/settingspages/SettingsPage.hpp"

namespace chatterino {

class DescriptionLabel;
class GeneralPageView;

class LimerinoPage : public SettingsPage
{
    Q_OBJECT

public:
    LimerinoPage();

    bool filterElements(const QString &query) override;

private:
    void initLayout(GeneralPageView &layout);
    void rebuildAuthSummary();

    GeneralPageView *view_{};
    DescriptionLabel *authSummaryLabel_{};
};

}  // namespace chatterino
