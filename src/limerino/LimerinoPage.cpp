#include "limerino/LimerinoPage.hpp"

#include "providers/limerino/LimerinoAuth.hpp"
#include "providers/limerino/commands/Identity.hpp"
#include "singletons/Settings.hpp"
#include "widgets/dialogs/LimerinoAuthDialog.hpp"
#include "widgets/settingspages/GeneralPageView.hpp"
#include "widgets/settingspages/SettingWidget.hpp"

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
    // Commands wiki (top of page, registry-driven; batches extend it).
    layout.addTitle("Limerino commands");
    layout.addDescription(QStringLiteral(
        "Commands ported from the reference plugin, running natively. "
        "Usage and backends are documented here."));
    for (const auto &doc : LimerinoCommands::commandDocs())
    {
        layout.addDescription(
            QStringLiteral("%1 - %2 - %3")
                .arg(doc.names, doc.usage, doc.description));
    }

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

    layout.addDescription(QStringLiteral(
        "Paste host used by list commands (/listfollows, /modlist, ...): "
        "your generated list output is uploaded publicly to this "
        "third-party service. Change only if you trust it."));
    SettingWidget::lineEdit(
        QStringLiteral("Paste host"), getSettings()->limerinoPasteHost,
        QStringLiteral("List output is uploaded to this service "
                       "(defaults to https://h.potat.app)"))
        ->addTo(layout);

    LimerinoAuth::accountsChanged.connect(
        [this] { this->rebuildAuthSummary(); }, this->managedConnections_);
    this->rebuildAuthSummary();

    layout.addStretch();
}

void LimerinoPage::onShow()
{
    // Opening the Limerino tab always refreshes the auth cache (validity +
    // moderated-channel lists may have drifted since the last check).
    LimerinoAuth::refreshAccounts();
}

void LimerinoPage::rebuildAuthSummary()
{
    if (this->authSummaryLabel_ == nullptr)
    {
        return;
    }
    const auto accounts = LimerinoAuth::accounts();
    const auto s = LimerinoAuth::summary();
    QString text;
    if (accounts.isEmpty())
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
        for (const auto &a : accounts)
        {
            text += QStringLiteral("\n%1: %2, scopes: %3, checked: %4, "
                                   "channels: %5")
                        .arg(a.displayName.isEmpty() ? a.login : a.displayName)
                        .arg(a.valid ? QStringLiteral("valid")
                                     : QStringLiteral("invalid: ") + a.lastError)
                        .arg(a.scopes.isEmpty()
                                 ? QStringLiteral("(not validated yet)")
                                 : QString::number(a.scopes.size()))
                        .arg(a.lastValidatedAt.isValid()
                                 ? a.lastValidatedAt.toString(
                                       QStringLiteral("yyyy-MM-dd hh:mm"))
                                 : QStringLiteral("never"))
                        .arg(a.moderatedChannels.size());
        }
    }
    this->authSummaryLabel_->setText(text);
}

}  // namespace chatterino
