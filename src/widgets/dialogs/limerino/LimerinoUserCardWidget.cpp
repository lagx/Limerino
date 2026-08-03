// SPDX-License-Identifier: MIT

#include "widgets/dialogs/limerino/LimerinoUserCardWidget.hpp"

#include "providers/limerino/gql/LimerinoUserCardExtras.hpp"
#include "util/LayoutCreator.hpp"
#include "widgets/Label.hpp"

#include <QHBoxLayout>
#include <QPointer>

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
                   .setLayoutType<QHBoxLayout>()
                   .withoutMargin();
    box->setSpacing(0);

    box.emplace<Label>(QString(), FontStyle::UiMedium)
        .assign(&this->languageTagLabel_);
    QPalette tagPalette;
    tagPalette.setColor(QPalette::WindowText,
                        QColor(QString::fromLatin1(DIM_TEXT_COLOR)));
    this->languageTagLabel_->setPalette(tagPalette);

    // G3: teamLabel_, G4: subDetailLabel_ are vbox-row members - created and
    // populated in those batches. Deliberately not created here so G2's
    // UserInfoPopup-side hook stays one layout-insert site.

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

    // G3/G4: teams + subscription detail join here once those labels exist.
    this->setVisible(show);
    this->layout()->invalidate();
}

}  // namespace chatterino::limerino
