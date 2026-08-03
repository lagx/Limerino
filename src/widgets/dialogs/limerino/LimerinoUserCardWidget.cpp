// SPDX-License-Identifier: MIT

#include "widgets/dialogs/limerino/LimerinoUserCardWidget.hpp"

#include "providers/limerino/gql/LimerinoUserCardExtras.hpp"
#include "util/LayoutCreator.hpp"
#include "widgets/Label.hpp"

#include <QHBoxLayout>
#include <QPointer>
#include <QVBoxLayout>

namespace chatterino::limerino {

namespace gql = chatterino::LimerinoAuth::gql;

namespace {

// Match the neighbour (ui_.userIDLabel) exactly: dim-grey WindowText.
constexpr const char *DIM_TEXT_COLOR = "#aaa";

}  // namespace

LimerinoUserCardWidget::LimerinoUserCardWidget(QWidget *parent)
    : BaseWidget(parent)
{
    auto box = LayoutCreator<LimerinoUserCardWidget>(this)
                   .setLayoutType<QVBoxLayout>()
                   .withoutMargin();
    box->setSpacing(0);

    // Tag on the first row; team directly underneath; both this column's
    // right edge stays flush with the name/vbox column via the parent box.
    box.emplace<Label>(QString(), FontStyle::UiMedium)
        .assign(&this->languageTagLabel_);
    QPalette tagPalette;
    tagPalette.setColor(QPalette::WindowText,
                        QColor(QString::fromLatin1(DIM_TEXT_COLOR)));
    this->languageTagLabel_->setPalette(tagPalette);

    // G3: primary team - same right-aligned column, one row under the tag.
    box.emplace<Label>(QString(), FontStyle::UiMedium)
        .assign(&this->teamLabel_);
    QPalette teamPalette;
    teamPalette.setColor(QPalette::WindowText,
                         QColor(QString::fromLatin1(DIM_TEXT_COLOR)));
    this->teamLabel_->setPalette(teamPalette);

    // G4: no own label - the detail rides on the popup's existing subageLabel

    this->rebuild();
}

void LimerinoUserCardWidget::setTarget(const QString &userId,
                                       const QString &channelId,
                                       const QString &loginForSubRow)
{
    const bool changed = (userId != this->userId_) ||
                         (channelId != this->channelId_);

    this->userId_ = userId;
    this->channelId_ = channelId;
    this->loginForSubRow_ = loginForSubRow;

    // Any in-flight reply for a *previous* target is silently discarded via
    // the generation check (rule 8). No abort plumbing is needed - the
    // network layer outlives this widget harmlessly.
    ++this->requestGeneration_;

    if (!changed && !this->userId_.isEmpty())
    {
        return;  // same target - keep the current result/cache hit
    }

    if (userId.isEmpty() || userId.startsWith(QStringLiteral("kick:")))
    {
        this->extras_ = {};
        this->rebuild();
        return;
    }

    this->refetch();
}

void LimerinoUserCardWidget::refetch()
{
    const quint64 generation = this->requestGeneration_;
    QPointer<LimerinoUserCardWidget> self(this);
    const QString userId = this->userId_;
    const QString channelId = this->channelId_;

    this->extras_ = {};
    this->rebuild();

    gql::fetchUserCardExtras(
        userId, channelId,
        [self, generation](std::optional<gql::LimerinoUserCardExtras> extras) {
            if (!self || generation != self->requestGeneration_ ||
                !extras.has_value())
            {
                return;
            }
            self->extras_ = std::move(*extras);
            self->rebuild();
        });
}

void LimerinoUserCardWidget::rebuild()
{
    // Ruling: bare tag exactly as the API returns it; hidden (zero footprint)
    // when absent / after any failure.
    this->languageTagLabel_->setText(this->extras_.preferredLanguageTag);
    const bool show = !this->extras_.preferredLanguageTag.isEmpty();
    this->languageTagLabel_->setVisible(show);

    // Ruling 1: show the team's `name` only; nothing rendered (and no layout
    // space claimed) when the user is on no team or the field was null.
    this->teamLabel_->setText(this->extras_.primaryTeamName);
    const bool showTeam = !this->extras_.primaryTeamName.isEmpty();
    this->teamLabel_->setVisible(showTeam);

    // G4: subscription detail is appended by the caller to its own subage row.
    this->setVisible(show || showTeam);
    this->layout()->invalidate();
}

QString LimerinoUserCardWidget::subscriptionSuffix() const
{
    const auto &sub = this->extras_.subscription;
    if (!sub.has_value())
    {
        return {};
    }

    QStringList parts;

    // Ruling: map only plat­form values with real evidence (raw was ruled, but
    // un-derivable wire strings are hidden field-by-field rather than shown).
    const QString &platform = sub->platform;
    if (platform == QLatin1String("web"))
    {
        parts << QStringLiteral("web");
    }
    else if (platform == QLatin1String("android"))
    {
        parts << QStringLiteral("Android");
    }
    else if (platform == QLatin1String("ios"))
    {
        parts << QStringLiteral("iOS");
    }
    else if (platform == QLatin1String("prime") || sub->purchasedWithPrime)
    {
        parts << QStringLiteral("Prime");
    }
    // any other platform string -> degraded to nothing (field-by-field)

    if (sub->isGift)
    {
        parts << QStringLiteral("gift");
    }
    if (sub->tier == QLatin1String("2000"))
    {
        parts << QStringLiteral("Tier 2");
    }
    else if (sub->tier == QLatin1String("3000"))
    {
        parts << QStringLiteral("Tier 3");
    }
    // "1000" / unknown -> existing sub-age row already shows tier; no duplicate

    if (parts.isEmpty())
    {
        return {};
    }
    return QStringLiteral(" · ") + parts.join(QStringLiteral(", "));
}

}  // namespace chatterino::limerino
