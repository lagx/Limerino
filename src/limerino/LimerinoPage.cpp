#include "limerino/LimerinoPage.hpp"

#include "widgets/settingspages/GeneralPageView.hpp"

#include <QFrame>
#include <QHBoxLayout>
#include <QVBoxLayout>

namespace chatterino {

LimerinoPage::LimerinoPage()
{
    auto *y = new QVBoxLayout;
    auto *x = new QHBoxLayout;
    auto *view = GeneralPageView::withNavigation(this);
    this->view_ = view;
    x->addWidget(view);
    auto *z = new QFrame;
    z->setLayout(x);
    y->addWidget(z);
    this->setLayout(y);

    this->initLayout(*view);
}

bool LimerinoPage::filterElements(const QString &query)
{
    if (this->view_)
    {
        return this->view_->filterElements(query) || query.isEmpty();
    }
    else
    {
        return false;
    }
}

void LimerinoPage::initLayout(GeneralPageView &layout)
{
    layout.addTitle("Limerino");
    layout.addDescription(QStringLiteral(
        R"(<a href="https://github.com/lagx/Limerino">Limerino</a> is a Chatterino fork built on top of technorino.)"));
    layout.addDescription(QStringLiteral(
        "Limerino-specific settings and features will live on this page."));

    layout.addStretch();
}

}  // namespace chatterino
