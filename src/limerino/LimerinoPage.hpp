#pragma once

#include "widgets/settingspages/SettingsPage.hpp"

namespace chatterino {

class GeneralPageView;

class LimerinoPage : public SettingsPage
{
    Q_OBJECT

public:
    LimerinoPage();

    bool filterElements(const QString &query) override;

private:
    void initLayout(GeneralPageView &layout);

    GeneralPageView *view_{};
};

}  // namespace chatterino
