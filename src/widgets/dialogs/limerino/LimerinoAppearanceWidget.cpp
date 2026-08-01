// SPDX-License-Identifier: MIT

#include "widgets/dialogs/limerino/LimerinoAppearanceWidget.hpp"

#include "Application.hpp"
#include "controllers/accounts/AccountController.hpp"
#include "providers/limerino/gql/LimerinoGql.hpp"
#include "providers/limerino/gql/PersistedQueries.hpp"
#include "providers/limerino/LimerinoAuth.hpp"
#include "providers/limerino/LimerinoErrors.hpp"
#include "providers/twitch/api/Helix.hpp"
#include "providers/twitch/TwitchAccount.hpp"
#include "providers/twitch/TwitchBadges.hpp"
#include "providers/twitch/TwitchChannel.hpp"
#include "util/DisplayBadge.hpp"
#include "util/Twitch.hpp"
#include "widgets/splits/Split.hpp"

#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QPointer>
#include <QPushButton>
#include <QVBoxLayout>

namespace chatterino::limerino {

namespace gql = chatterino::LimerinoAuth::gql;

LimerinoAppearanceWidget::LimerinoAppearanceWidget(Split *split)
    : BaseWidget(split)
    , split_(split)
{
    auto *root = new QVBoxLayout(this);

    // ---- badge ----
    auto *badgeBox = new QGroupBox(QStringLiteral("Badge"), this);
    auto *badgeForm = new QFormLayout(badgeBox);

    this->badgeCombo_ = new QComboBox(badgeBox);
    badgeForm->addRow(QStringLiteral("Badge"), this->badgeCombo_);

    this->scopeCombo_ = new QComboBox(badgeBox);
    this->scopeCombo_->addItem(QStringLiteral("Global (all channels)"));
    this->scopeCombo_->addItem(QStringLiteral("This channel (not available)"));
    this->scopeCombo_->setItemData(
        1, QStringLiteral("Twitch doesn't offer a per-channel badge API"),
        Qt::ToolTipRole);
    this->scopeCombo_->model()->setData(
        this->scopeCombo_->model()->index(1, 0), QVariant(false),
        Qt::UserRole - 1);  // disable the channel row
    badgeForm->addRow(QStringLiteral("Apply to"), this->scopeCombo_);

    this->previewLabel_ = new QLabel(badgeBox);
    badgeForm->addRow(QStringLiteral("Preview"), this->previewLabel_);

    this->applyBadgeButton_ =
        new QPushButton(QStringLiteral("Apply badge"), badgeBox);
    badgeForm->addRow(this->applyBadgeButton_);
    root->addWidget(badgeBox);

    // ---- chat color ----
    auto *colorBox = new QGroupBox(QStringLiteral("Chat color"), this);
    auto *colorForm = new QFormLayout(colorBox);

    this->colorCombo_ = new QComboBox(colorBox);
    this->colorCombo_->addItems(VALID_HELIX_COLORS);
    colorForm->addRow(QStringLiteral("Color"), this->colorCombo_);

    this->colorHexEdit_ = new QLineEdit(colorBox);
    this->colorHexEdit_->setPlaceholderText(
        QStringLiteral("or #rrggbb (Turbo/Prime only)"));
    colorForm->addRow(QStringLiteral("Custom"), this->colorHexEdit_);

    this->colorPreviewLabel_ = new QLabel(colorBox);
    colorForm->addRow(QStringLiteral("Preview"), this->colorPreviewLabel_);

    this->applyColorButton_ =
        new QPushButton(QStringLiteral("Apply color"), colorBox);
    colorForm->addRow(this->applyColorButton_);
    root->addWidget(colorBox);

    this->statusLabel_ = new QLabel(this);
    this->statusLabel_->setWordWrap(true);
    root->addWidget(this->statusLabel_);
    root->addStretch(1);

    QObject::connect(this->badgeCombo_,
                     QOverload<int>::of(&QComboBox::currentIndexChanged),
                     this, [this] { this->refreshPreview(); });
    QObject::connect(this->colorCombo_,
                     QOverload<int>::of(&QComboBox::currentIndexChanged),
                     this, [this] { this->refreshPreview(); });
    QObject::connect(this->colorHexEdit_, &QLineEdit::textChanged, this,
                     [this] { this->refreshPreview(); });
    QObject::connect(this->applyBadgeButton_, &QPushButton::clicked, this,
                     [this] { this->applyBadge(); });
    QObject::connect(this->applyColorButton_, &QPushButton::clicked, this,
                     [this] { this->applyColor(); });

    this->refreshBadges();
    this->refreshPreview();
}

void LimerinoAppearanceWidget::refreshBadges()
{
    auto *tchan = dynamic_cast<TwitchChannel *>(
        this->split_->getSelectedChannel().get());
    if (tchan == nullptr)
    {
        this->statusLabel_->setText(QStringLiteral("not a twitch channel"));
        return;
    }

    QString err;
    auto token = LimerinoAuth::resolveReadToken(&err);
    if (!token.hasToken())
    {
        this->statusLabel_->setText(
            err.isEmpty() ? LimerinoAuth::errors::tokenRequiredMessage(
                                QStringLiteral("pick your badges"))
                          : err);
        return;
    }

    gql::executePersisted(
        gql::PQ_CHAT_SETTINGS_BADGES,
        QJsonObject{{QStringLiteral("channelLogin"), tchan->getName()}},
        token.token,
        [g = QPointer<LimerinoAppearanceWidget>(this), this](
            const QJsonObject &data) {
            if (!g)
            {
                return;
            }
            this->badgeCombo_->clear();

            const QJsonArray badges =
                data[QStringLiteral("currentUser")].toObject()[
                    QStringLiteral("availableBadges")].toArray();
            QList<DisplayBadge> items;
            for (const QJsonValue &v : badges)
            {
                const QString setId =
                    v.toObject()[QStringLiteral("setID")].toString();
                if (setId.isEmpty())
                {
                    continue;
                }
                // Preview label: badge(setID) via native badge lookup,
                // user data: setID for the select op.
                items.append(DisplayBadge(setId, setId));
            }
            for (const DisplayBadge &item : items)
            {
                this->badgeCombo_->addItem(item.displayName(),
                                           item.badgeName());
            }
            getApp()->getTwitchBadges()->getBadgeIcons(
                items, [combo = QPointer<QComboBox>(this->badgeCombo_)](
                             QString identifier, const auto &icon) {
                    if (!combo)
                    {
                        return;
                    }
                    const int idx = combo->findData(identifier);
                    if (idx >= 0 && icon)
                    {
                        combo->setItemIcon(idx, *icon);
                    }
                });
            if (items.isEmpty())
            {
                this->statusLabel_->setText(
                    QStringLiteral("No badges available in this channel."));
            }
            this->refreshPreview();
        },
        [g = QPointer<LimerinoAppearanceWidget>(this)](const gql::GqlError &e) {
            if (g)
            {
                g->statusLabel_->setText(e.message);
            }
        });
}

void LimerinoAppearanceWidget::refreshPreview()
{
    const auto self = getApp()->getAccounts()->twitch.getCurrent();
    const QString name =
        self && !self->isAnon() ? self->getUserName() : QStringLiteral("you");

    // Name shown in the selected color next to the chosen badge.
    QString colorName = this->colorCombo_->currentText();
    const QString hex = this->colorHexEdit_->text().trimmed();
    if (!hex.isEmpty())
    {
        colorName = hex;
    }
    const QColor color(colorName);
    this->previewLabel_->setText(
        QStringLiteral("%1's messages will show the selected badge")
            .arg(name));
    if (color.isValid())
    {
        this->colorPreviewLabel_->setText(
            QStringLiteral("<span style=\"color:%1\">%2: hello this is a preview</span>")
                .arg(color.name(), name));
    }
    else
    {
        this->colorPreviewLabel_->setText(
            QStringLiteral("(invalid color)"));
    }
}

void LimerinoAppearanceWidget::applyBadge()
{
    const QString setId = this->badgeCombo_->currentData().toString();
    if (setId.isEmpty())
    {
        this->statusLabel_->setText(QStringLiteral("pick a badge first"));
        return;
    }

    QString err;
    auto token = LimerinoAuth::resolveReadToken(&err);
    if (!token.hasToken())
    {
        this->statusLabel_->setText(
            err.isEmpty() ? LimerinoAuth::errors::tokenRequiredMessage(
                                QStringLiteral("select your badges"))
                          : err);
        return;
    }

    // Plugin's input shape, verbatim: badgeSetID + badgeSetVersion "1".
    gql::executePersisted(
        gql::PQ_SELECT_GLOBAL_BADGE,
        QJsonObject{{QStringLiteral("input"),
                     QJsonObject{{QStringLiteral("badgeSetID"), setId},
                                 {QStringLiteral("badgeSetVersion"),
                                  QStringLiteral("1")}}}},
        token.token,
        [g = QPointer<LimerinoAppearanceWidget>(this)](
            const QJsonObject &data) {
            if (!g)
            {
                return;
            }
            // Plugin's success/failure shape: data.ChatSettings_SelectGlobalBadge.error.code
            const QString code =
                data[QStringLiteral("ChatSettings_SelectGlobalBadge")]
                    .toObject()[QStringLiteral("error")]
                    .toObject()[QStringLiteral("code")]
                    .toString();
            g->statusLabel_->setText(
                code.isEmpty()
                    ? QStringLiteral("Successfully selected global badge!")
                    : QStringLiteral("Unable to select global badge! Status: %1")
                          .arg(code));
        },
        [g = QPointer<LimerinoAppearanceWidget>(this)](const gql::GqlError &e) {
            if (g)
            {
                g->statusLabel_->setText(
                    QStringLiteral("Unable to select global badge! Status: %1")
                        .arg(e.message));
            }
        });
}

void LimerinoAppearanceWidget::applyColor()
{
    auto self = getApp()->getAccounts()->twitch.getCurrent();
    if (!self || self->isAnon())
    {
        this->statusLabel_->setText(QStringLiteral(
            "You must be logged in (main account) to change your chat color."));
        return;
    }

    QString colorString = this->colorHexEdit_->text().trimmed();
    if (colorString.isEmpty())
    {
        colorString = this->colorCombo_->currentText();
    }
    cleanHelixColorName(colorString);

    const QPointer<LimerinoAppearanceWidget> g(this);
    getHelix()->updateUserChatColor(
        self->getUserId(), colorString,
        [g, colorString] {
            if (g)
            {
                g->statusLabel_->setText(
                    QStringLiteral("Your color has been changed to %1.")
                        .arg(colorString));
            }
        },
        [g, colorString](auto error, auto /*message*/) {
            if (!g)
            {
                return;
            }
            QString note;
            if (error == HelixUpdateUserChatColorError::UserMissingScope)
            {
                note = QStringLiteral(
                    " (missing scope - re-login with your main account)");
            }
            else if (error == HelixUpdateUserChatColorError::InvalidColor)
            {
                note = QStringLiteral(
                    " (invalid color - custom hex codes require Turbo/Prime)");
            }
            g->statusLabel_->setText(
                QStringLiteral("Failed to change color to %1%2.")
                    .arg(colorString, note));
        });
}

}  // namespace chatterino::limerino
