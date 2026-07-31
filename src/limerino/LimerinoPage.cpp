#include "limerino/LimerinoPage.hpp"

#include "providers/limerino/LimerinoAuth.hpp"
#include "widgets/dialogs/LimerinoAuthDialog.hpp"
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

    layout.addTitle("Extra features");
    layout.addDescription(QStringLiteral(
        "Options for extra features (moderation, rewards, chat settings) "
        "that need a second, elevated sign-in. Stored separately from your "
        "main account."));
    this->authSummaryLabel_ = layout.addDescription(QString());
    layout.addButton(QStringLiteral("Manage extra-features login..."),
                     [this] {
                         auto *dialog = new LimerinoAuthDialog(this);
                         dialog->show();
                     });

    LimerinoAuth::accountsChanged.connect(
        [this] { this->rebuildAuthSummary(); }, this->managedConnections_);
    this->rebuildAuthSummary();

    layout.addStretch();
}

void LimerinoPage::rebuildAuthSummary()
{
    if (this->authSummaryLabel_ == nullptr)
    {
        return;
    }
    const auto s = LimerinoAuth::summary();
    QString text;
    if (s.accountCount == 0)
    {
        text = QStringLiteral("No extra-features accounts signed in.");
    }
    else
    {
        text = QStringLiteral("Signed in to %1 account(s). Extra features "
                              "available in %2 channels.")
                   .arg(s.accountCount)
                   .arg(s.moderatedChannelCount);
        if (s.invalidAccountCount > 0)
        {
            text += QStringLiteral(" (%1 account(s) need attention, see "
                                   "Manage extra-features login)")
                        .arg(s.invalidAccountCount);
        }
    }
    this->authSummaryLabel_->setText(text);
}

}  // namespace chatterino
