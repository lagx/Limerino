// SPDX-License-Identifier: MIT
// Dialog for the secondary "Limerino Extra Features" auth.
// Deliberately a NEW dialog; the stock LoginDialog is untouched.

#include "widgets/dialogs/LimerinoAuthDialog.hpp"

#include "providers/limerino/LimerinoAuth.hpp"
#include "util/Clipboard.hpp"

#include <QDesktopServices>
#include <QFile>
#include <QFontDatabase>
#include <QFormLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTabWidget>
#include <QTableWidget>
#include <QTextEdit>
#include <QUrl>
#include <QVBoxLayout>

namespace chatterino {

namespace {

// Read-only copy of the frozen auth script, shared by all dialog instances.
QString scriptText()
{
    static const QString text = [] {
        QFile f(QStringLiteral(":/limerino/limerinoauth.txt"));
        if (f.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            return QString::fromUtf8(f.readAll());
        }
        return QStringLiteral("(could not load :/limerino/limerinoauth.txt)");
    }();
    return text;
}

QString channelListText(const LimerinoAuth::LimerinoAuthAccount &account)
{
    QStringList names;
    for (const auto &c : account.moderatedChannels)
    {
        names.append(c.displayName.isEmpty() ? c.login : c.displayName);
    }
    return names.join(QStringLiteral(", "));
}

}  // namespace

LimerinoAuthDialog::LimerinoAuthDialog(QWidget *parent)
    : BasePopup({BaseWindow::Flags::Dialog}, parent)
{
    this->setWindowTitle(QStringLiteral("Limerino - Extra features login"));
    this->resize(520, 480);
    this->setAttribute(Qt::WA_DeleteOnClose);

    auto *root = new QVBoxLayout(this);

    this->ui_.tabs = new QTabWidget(this);

    // ------------------------- Device Login tab -------------------------
    auto *devicePage = new QWidget(this);
    auto *deviceLayout = new QVBoxLayout(devicePage);

    auto *deviceInfo = new QLabel(
        QStringLiteral(
            "Sign in with a second, elevated authorization used only for "
            "extra features (moderation, rewards, chat settings). It is "
            "stored separately from your main account."),
        devicePage);
    deviceInfo->setWordWrap(true);
    deviceLayout->addWidget(deviceInfo);

    this->ui_.deviceStatus = new QLabel(devicePage);
    this->ui_.deviceStatus->setWordWrap(true);
    deviceLayout->addWidget(this->ui_.deviceStatus);

    this->ui_.deviceCode = new QLabel(devicePage);
    QFont codeFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    codeFont.setPointSize(14);
    this->ui_.deviceCode->setFont(codeFont);
    deviceLayout->addWidget(this->ui_.deviceCode);

    auto *deviceButtons = new QHBoxLayout;
    this->ui_.deviceStart = new QPushButton(QStringLiteral("Start login"),
                                            devicePage);
    this->ui_.deviceCancel = new QPushButton(QStringLiteral("Cancel"),
                                             devicePage);
    this->ui_.deviceCancel->setEnabled(false);
    deviceButtons->addWidget(this->ui_.deviceStart);
    deviceButtons->addWidget(this->ui_.deviceCancel);
    deviceButtons->addStretch(1);
    deviceLayout->addLayout(deviceButtons);
    deviceLayout->addStretch(1);

    // ------------------------- Script Login tab -------------------------
    auto *scriptPage = new QWidget(this);
    auto *scriptLayout = new QVBoxLayout(scriptPage);

    auto *scriptInfo = new QLabel(
        QStringLiteral(
            "If the in-app device login is unavailable, run this script "
            "with node (node limerinoauth.txt) and paste the access token "
            "it prints below. Running the script requires node on your PATH."),
        scriptPage);
    scriptInfo->setWordWrap(true);
    scriptLayout->addWidget(scriptInfo);

    auto *scriptView = new QTextEdit(scriptPage);
    scriptView->setReadOnly(true);
    scriptView->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    scriptView->setPlainText(scriptText());
    scriptLayout->addWidget(scriptView, 1);

    auto *pasteRow = new QHBoxLayout;
    this->ui_.scriptTokenInput = new QLineEdit(scriptPage);
    this->ui_.scriptTokenInput->setEchoMode(QLineEdit::Password);
    this->ui_.scriptTokenInput->setPlaceholderText(
        QStringLiteral("paste access token (from the script)"));
    auto *scriptAdd =
        new QPushButton(QStringLiteral("Add account"), scriptPage);
    pasteRow->addWidget(this->ui_.scriptTokenInput, 1);
    pasteRow->addWidget(scriptAdd);
    scriptLayout->addLayout(pasteRow);

    // --------------------------- Accounts tab ---------------------------
    auto *accountsPage = new QWidget(this);
    auto *accountsLayout = new QVBoxLayout(accountsPage);

    this->ui_.accountsTable = new QTableWidget(0, 4, accountsPage);
    this->ui_.accountsTable->setHorizontalHeaderLabels(
        {QStringLiteral("Account"), QStringLiteral("Extra-feature channels"),
         QStringLiteral("Status"), QStringLiteral("Remove")});
    this->ui_.accountsTable->horizontalHeader()->setSectionResizeMode(
        0, QHeaderView::ResizeToContents);
    this->ui_.accountsTable->horizontalHeader()->setSectionResizeMode(
        1, QHeaderView::Stretch);
    this->ui_.accountsTable->horizontalHeader()->setSectionResizeMode(
        2, QHeaderView::ResizeToContents);
    this->ui_.accountsTable->verticalHeader()->setVisible(false);
    this->ui_.accountsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    this->ui_.accountsTable->setSelectionMode(QAbstractItemView::NoSelection);
    this->ui_.accountsTable->setFocusPolicy(Qt::NoFocus);
    this->ui_.accountsTable->setAlternatingRowColors(true);
    accountsLayout->addWidget(this->ui_.accountsTable, 1);

    auto *accountsFooter = new QHBoxLayout;
    this->ui_.accountsSummary = new QLabel(accountsPage);
    auto *refreshButton =
        new QPushButton(QStringLiteral("Re-check accounts"), accountsPage);
    accountsFooter->addWidget(this->ui_.accountsSummary, 1);
    accountsFooter->addWidget(refreshButton);
    accountsLayout->addLayout(accountsFooter);

    this->ui_.tabs->addTab(devicePage, QStringLiteral("Device Login"));
    this->ui_.tabs->addTab(scriptPage, QStringLiteral("Script Login"));
    this->ui_.tabs->addTab(accountsPage, QStringLiteral("Accounts"));
    root->addWidget(this->ui_.tabs);

    // ------------------------------- logic ------------------------------

    this->deviceLogin_ = new LimerinoAuth::DeviceLogin(this);

    QObject::connect(
        this->deviceLogin_, &LimerinoAuth::DeviceLogin::statusChanged, this,
        [this](const LimerinoAuth::DeviceLogin::Status &status) {
            this->setDeviceStatusText(status);
        });

    QObject::connect(this->ui_.deviceStart, &QPushButton::clicked, this, [this] {
        this->deviceLogin_->start();
        this->ui_.deviceCancel->setEnabled(true);
    });
    QObject::connect(this->ui_.deviceCancel, &QPushButton::clicked, this,
                     [this] {
                         this->deviceLogin_->cancel();
                         this->ui_.deviceCancel->setEnabled(false);
                     });

    QObject::connect(scriptAdd, &QPushButton::clicked, this, [this] {
        const QString token =
            LimerinoAuth::normalizeToken(this->ui_.scriptTokenInput->text());
        if (token.isEmpty())
        {
            return;
        }
        LimerinoAuth::addOrUpdateToken({token, {}, {}, true});
        this->ui_.scriptTokenInput->clear();
        // Clipboard hygiene: the token material just came from the clipboard.
        crossPlatformCopy(QString());
        this->ui_.tabs->setCurrentIndex(2);
    });

    QObject::connect(refreshButton, &QPushButton::clicked, this, [this] {
        this->ui_.accountsSummary->setText(QStringLiteral("Checking..."));
        LimerinoAuth::refreshAccounts(
            [this](const LimerinoAuth::LimerinoAuthRefreshResult &r) {
                QString text = QStringLiteral("%1 valid, %2 invalid of %3 "
                                              "accounts; extra features in %4 "
                                              "channels")
                                   .arg(r.valid)
                                   .arg(r.invalid)
                                   .arg(r.total)
                                   .arg(r.moderatedChannels);
                if (!r.errors.isEmpty())
                {
                    text += QStringLiteral(" - ") + r.errors.join("; ");
                }
                this->ui_.accountsSummary->setText(text);
            });
    });

    LimerinoAuth::accountsChanged.connect(
        [this] { this->rebuildAccountsTable(); },
        this->managedConnections_);

    this->setDeviceStatusText(this->deviceLogin_->status());
    this->rebuildAccountsTable();
}

void LimerinoAuthDialog::setDeviceStatusText(
    const LimerinoAuth::DeviceLogin::Status &status)
{
    using State = LimerinoAuth::DeviceLogin::State;

    switch (status.state)
    {
        case State::WaitingForUser: {
            // Required deviation from Moltorino: we copy the user code AND
            // say we did, in the same action.
            crossPlatformCopy(status.userCode);
            this->ui_.deviceStatus->setText(
                status.message +
                QStringLiteral(" The code was copied to your clipboard."));
            this->ui_.deviceCode->setText(status.userCode);
            this->ui_.deviceStart->setEnabled(false);
            break;
        }
        case State::RequestingCode:
            this->ui_.deviceStatus->setText(status.message);
            this->ui_.deviceStart->setEnabled(false);
            break;
        case State::Idle:
            this->ui_.deviceStatus->setText(QStringLiteral("Not signed in."));
            this->ui_.deviceCode->clear();
            this->ui_.deviceStart->setEnabled(true);
            this->ui_.deviceCancel->setEnabled(false);
            break;
        default:
            // Authorized, Denied, Expired, Failed - message only.
            this->ui_.deviceStatus->setText(status.message);
            this->ui_.deviceStart->setEnabled(true);
            this->ui_.deviceCancel->setEnabled(false);
            break;
    }

    if (status.secondsRemaining > 0 &&
        status.state == State::WaitingForUser)
    {
        this->ui_.deviceStatus->setText(
            this->ui_.deviceStatus->text() +
            QStringLiteral(" (%1 s left)").arg(status.secondsRemaining));
    }
}

void LimerinoAuthDialog::rebuildAccountsTable()
{
    const auto accounts = LimerinoAuth::accounts();
    this->ui_.accountsTable->setRowCount(0);
    this->ui_.accountsTable->setRowCount(int(accounts.size()));

    int row = 0;
    for (const auto &account : accounts)
    {
        const QString title = account.displayName.isEmpty()
                                  ? account.login
                                  : account.displayName;
        auto *accountItem =
            new QTableWidgetItem(QStringLiteral("%1 (ID %2)")
                                     .arg(title, account.userId));
        auto *channelsItem =
            new QTableWidgetItem(channelListText(account));

        QString statusText =
            account.valid
                ? QStringLiteral("valid, %1 scopes, checked %2")
                      .arg(account.scopes.size())
                      .arg(account.lastValidatedAt.isValid()
                               ? account.lastValidatedAt.toString(
                                     QStringLiteral("yyyy-MM-dd hh:mm"))
                               : QStringLiteral("never"))
                : QStringLiteral("invalid: %1").arg(account.lastError);
        auto *statusItem = new QTableWidgetItem(statusText);

        this->ui_.accountsTable->setItem(row, 0, accountItem);
        this->ui_.accountsTable->setItem(row, 1, channelsItem);
        this->ui_.accountsTable->setItem(row, 2, statusItem);

        const QString userId = account.userId;
        auto *removeButton = new QPushButton(QStringLiteral("Remove"),
                                             this->ui_.accountsTable);
        QObject::connect(removeButton, &QPushButton::clicked, this,
                         [this, userId, title] {
                             const auto answer = QMessageBox::question(
                                 this, QStringLiteral("Remove account"),
                                 QStringLiteral("Remove the extra-features "
                                                "login for %1?")
                                     .arg(title));
                             if (answer == QMessageBox::Yes)
                             {
                                 LimerinoAuth::removeAccount(userId);
                             }
                         });
        this->ui_.accountsTable->setCellWidget(row, 3, removeButton);
        ++row;
    }

    const auto s = LimerinoAuth::summary();
    this->ui_.accountsSummary->setText(
        QStringLiteral("%1 accounts (%2 valid). Extra features available in "
                       "%3 channels.")
            .arg(s.accountCount)
            .arg(s.validAccountCount)
            .arg(s.moderatedChannelCount));
}

}  // namespace chatterino
